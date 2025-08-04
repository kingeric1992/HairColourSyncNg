#include "plugin.h"

static void appendHairShader(RE::NiProperty* prop, std::vector<RE::BSShaderProperty*>& shaders)
{
	if (!prop || prop->GetType() != RE::NiProperty::Type::kShade)
		return; 
	if (auto shader = static_cast<RE::BSShaderProperty*>(prop);
		shader->material && shader->material->GetFeature() == RE::BSShaderMaterial::Feature::kHairTint)
		shaders.push_back(shader);
}
static void findHairShaders(RE::NiAVObject* av, std::vector<RE::BSShaderProperty*>& shaders)
{
	assert(av);
	if (auto shape = av->AsTriShape())
		appendHairShader(shape->properties[RE::BSGeometry::States::kEffect].get(), shaders);
	else if (auto shape = av->AsNiTriShape())
		appendHairShader(shape->spEffectState.get(), shaders);
	else if (RE::NiNode* node = av->AsNode()) {
		for(auto& child : node->children)
			if (child)
				findHairShaders(child.get(), shaders);
	}
	static_assert(offsetof(RE::BSTriShape, properties[1]) == 0x128);
	static_assert(offsetof(RE::NiTriShape, spEffectState) == 0x118);
	static_assert(offsetof(RE::NiNode, children) == 0x110);
}
static RE::BGSColorForm* getHairColor(RE::TESNPC* base)
{
	assert(base);

	//Check in this record first
	if (base->headRelatedData && base->headRelatedData->hairColor)
		return base->headRelatedData->hairColor;

	//If no hair col, check template(s)
	if (base->faceNPC)
		return getHairColor(base->faceNPC);

	//If still no hair col, use race default
	if (base->race)
		if (auto face = base->race->faceRelatedData[
			base->actorData.actorBaseFlags.any(RE::ACTOR_BASE_DATA::Flag::kFemale)])
			return face->defaultHairColor;
	return nullptr;
}
static void attach(RE::NiNode* root, RE::TESNPC* npc)
{
	//This will look at every HairTint material under root and,
	//if its tint color does not match the colour of npc's hair,
	//replace it with a new material.
	auto* hairColor = getHairColor(npc);

	if (!hairColor)
		return;

	RE::NiColor col{
		hairColor->color.red / 128.0f,
		hairColor->color.green / 128.0f,
		hairColor->color.blue / 128.0f
	};

	std::vector<RE::BSShaderProperty*> shaders;
	findHairShaders(root, shaders);

	for (auto&& shader : shaders) {
		//shaders should only contain HairTint materials at this point
		assert(shader->material && shader->material->GetFeature() == RE::BSShaderMaterial::Feature::kHairTint);

		//I don't see exactly why this works (dividing by 128 instead of 255),
		//but it definitely does give us the right colour in game.
		// kingeric: these shader are applied as mult mask.
		if (RE::BSLightingShaderMaterialHairTint* oldMat, *newMat;
			(oldMat = static_cast<RE::BSLightingShaderMaterialHairTint*>(shader->material), oldMat->tintColor != col) && 
			(newMat = static_cast<RE::BSLightingShaderMaterialHairTint*>(shader->material->Create())))
		{	
			newMat->CopyMembers(shader->material);
			newMat->tintColor = col;

			//If newMaterial is not yet hashed (key == -1), BSShaderProperty::SetMaterial will 
			//compute the key, search the database for a match and use that material.
			//If no match is found, it will duplicate newMaterial, 
			//add the duplicate to the database and assign it to the shader.
			//The only way the shader ends up actually using newMaterial is if
			// a) newMaterial is hashed (key != -1) AND
			// b) param3 == false.
			//(param3 seems to indicate a material that might change over time)

			//The hash key is inited to -1 and not copied
			assert(newMat->hashKey == -1);

			//So this will actually assign a duplicate of newMaterial to the shader,
			//which may or may not have existed in the database prior to the call.
			shader->SetMaterial(newMat, false);

			//Thus, newMaterial will be unused at this point and must be cleaned up by us.
			//(note: the virtual dtor calls the Skyrim memory manager as appropriate)
			assert(shader->material != newMat);
			delete newMat;

			//The duplicate material assigned to the shader is managed by the resource handler 
			//and will be cleaned up by them when appropriate.
		}
	}
}
static void* AttachArmor_orig{};
static RE::NiAVObject* AttachArmor_hook(RE::BipedAnim* _this, RE::NiNode* armor, RE::NiNode* skeleton,
	void* a4, char a5, char a6, void* a7)
{
	if (armor && skeleton && skeleton->userData && skeleton->userData->data.objectReference
		&& skeleton->userData->data.objectReference->Is(RE::FormType::NPC)) {
		attach(armor, static_cast<RE::TESNPC*>(skeleton->userData->data.objectReference));
	}
	return reinterpret_cast<decltype(&AttachArmor_hook)>(AttachArmor_orig)(_this, armor, skeleton, a4, a5, a6, a7);
}
void hook() 
{
	auto& trampoline = SKSE::GetTrampoline();
	// (1.5.97) bipedAnim::attachArmor_1401CAFB0
	//.text:00000001401CAFB0 44 89 4C 24 20 | mov     dword ptr[rsp - 8 + a4], r9d
	//.text:00000001401CAFB5 4C 89 44 24 18 | mov     [rsp - 8 + a1], r8

	// (1.6.1170) bipedAnim::attachArmor_140217890
	//.text:0000000140217890 44 89 4C 24 20 | mov     dword ptr [rsp-8+arg_18], r9d
	//.text:0000000140217895 4C 89 44 24 18 | mov     [rsp-8+arg_10], r8
	auto pFunc = REL::Relocation<decltype(&AttachArmor_hook)>{ RELOCATION_ID(15535, 15712) }.address();

#pragma pack(1)
	struct { BYTE db[5]; UINT8 jmp; INT32 rel32; } aob{
		{ 0x44, 0x89, 0x4C, 0x24, 0x20 }, 0xE9, 0}; //jmp to pFunc + 5 

	if(memcmp(&aob, (void*)pFunc, 5))
		SKSE::stl::report_and_fail(std::format("AOB missmatch {}", prnAOB(aob.db)), SOURCE("plugin.cpp"));

	if(AttachArmor_orig = trampoline.allocate(sizeof(aob)); !AttachArmor_orig)
		SKSE::stl::report_and_fail("Failed to allocate memory for AttachArmor hook", SOURCE("plugin.cpp"));

	aob.rel32 = (INT32)((uintptr_t)pFunc + sizeof(aob.db) - (uintptr_t)AttachArmor_orig - sizeof(aob));
	memcpy_s(AttachArmor_orig, sizeof(aob), &aob, sizeof(aob));

	trampoline.write_branch<5>(pFunc, (uintptr_t)AttachArmor_hook);
}