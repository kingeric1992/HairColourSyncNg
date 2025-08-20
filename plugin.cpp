#include "plugin.h"
#include <Psapi.h>

// return true if found any.
template<class T>
static bool findHairShaders(RE::NiAVObject* av, T&& fn)
{
	assert(av);

	bool res = false;

	RE::NiProperty* prop{};
	if (auto shape = av->AsTriShape())
		prop = shape->GetGeometryRuntimeData().properties[RE::BSGeometry::States::kEffect].get();
	else if (auto shape = av->AsNiTriShape())
		prop = shape->GetRuntimeData().spEffectState.get();
	else if (RE::NiNode* node = av->AsNode()) {
		for (auto& child : node->GetChildren())
			if (child)
				res |= findHairShaders(child.get(), fn);
	}
	if (!prop || prop->GetType() != RE::NiProperty::Type::kShade)
		return res;
	return res |= fn(static_cast<RE::BSShaderProperty*>(prop));
}
// vr compatible build does not support static offset
//static_assert(offsetof(RE::BSTriShape, properties[1]) == 0x128);
//static_assert(offsetof(RE::NiTriShape, spEffectState) == 0x118);
//static_assert(offsetof(RE::NiNode, children) == 0x110);

static RE::BGSColorForm* getHairColor(RE::TESNPC* base)
{
	if (!base)
		return nullptr;

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
static bool attach(RE::NiAVObject* root, RE::BGSColorForm* hairCol)
{
	//This will look at every HairTint material under root and,
	//if its tint color does not match the colour of npc's hair,
	//replace it with a new material.

	RE::NiColor col{hairCol->color};
	col *= 2; // confirmed in game disambly.

	return findHairShaders(root, [&col](RE::BSShaderProperty* shader) {
		RE::BSShaderMaterial* mat;
		RE::BSLightingShaderMaterialHairTint* oldMat, * newMat;
		if (shader && (mat = shader->GetBaseMaterial()) && 
			mat->GetFeature() == RE::BSShaderMaterial::Feature::kHairTint &&
			(oldMat = static_cast<RE::BSLightingShaderMaterialHairTint*>(mat), oldMat->tintColor != col) &&
			(newMat = static_cast<RE::BSLightingShaderMaterialHairTint*>(mat->Create())) )
		{
			newMat->CopyMembers(mat);
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
			assert(mat != newMat);
			delete newMat;

			//The duplicate material assigned to the shader is managed by the resource handler 
			//and will be cleaned up by them when appropriate.
			return true;
		}
		return false;
	});
}
static void* AttachArmor_orig{};
static RE::NiAVObject* AttachArmor_hook(RE::BipedAnim* _this, RE::NiNode* armor, RE::NiNode* skeleton,
	void* a4, char a5, char a6, void* a7)
{
	RE::TESNPC* npc;
	RE::BGSColorForm* hair;
	if (auto ref = armor && skeleton ? skeleton->GetUserData() : nullptr; ref && ref->GetObjectReference() &&
		(hair = getHairColor(npc = ref->GetObjectReference()->As<RE::TESNPC>()))) // only proceed if have hair.
	{
		if (attach(armor, hair))
			npc->SetHairColor(hair); // in case hair is not set at npc node.
	}
	return reinterpret_cast<decltype(&AttachArmor_hook)>(AttachArmor_orig)(_this, armor, skeleton, a4, a5, a6, a7);
}
static uintptr_t getModuleOffset(uintptr_t pFunc, char* name, size_t size) {
	__try { // probably safer?	
		auto getModule = [](uintptr_t addr, char* cbuf, size_t ss) [[msvc::forceinline]] -> uintptr_t {
			DWORD cbNeeded;
			auto proc = GetCurrentProcess();
			if (HMODULE hmods[1024]; EnumProcessModules(proc, hmods, sizeof(hmods), &cbNeeded))
				for (int i = 0; i < (cbNeeded / sizeof(HMODULE)); ++i)
					if (MODULEINFO modInfo; GetModuleInformation(proc, hmods[i], &modInfo, sizeof(modInfo)) &&
						(uintptr_t)modInfo.lpBaseOfDll <= addr && addr < ((uintptr_t)modInfo.lpBaseOfDll + modInfo.SizeOfImage))
					{
						//char szModName[MAX_PATH];
						GetModuleFileName(hmods[i], cbuf, ss);
						return addr - (uintptr_t)modInfo.lpBaseOfDll;
					}
			return { };
		};
		auto unwrap = [](uintptr_t addr) [[msvc::forceinline]] -> uintptr_t {
			return *(WORD*)(addr) == 0x25FF ? (addr + 6) /* rip */ + *(INT32*)(addr + 2) /* rel32 */ : NULL;
		};
		if (auto target = unwrap(pFunc); target || (target = unwrap(pFunc + 5 + *(INT32*)(pFunc + 1))))
			return getModule(*(uintptr_t*)target, name, size);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
	}
	return {};
}
void hook()
{
	// (1.5.97) bipedAnim::attachArmor_1401CAFB0
	//.text:00000001401CAFB0 44 89 4C 24 20 | mov     dword ptr[rsp - 8 + a4], r9d
	//.text:00000001401CAFB5 4C 89 44 24 18 | mov     [rsp - 8 + a1], r8

	// (1.6.1170) bipedAnim::attachArmor_140217890
	//.text:0000000140217890 44 89 4C 24 20 | mov     dword ptr [rsp-8+arg_18], r9d
	//.text:0000000140217895 4C 89 44 24 18 | mov     [rsp-8+arg_10], r8

	// (1.4.15) bipedAnim::attachArmor_0x1401db9e0 (VR)
	//.text:00000001401DB9E0 44 89 4C 24 20 | mov     [rsp+20h], r9d
	//.text:00000001401DB9E5 4C 89 44 24 18 | mov     [rsp+18h], r8

	auto pfn = REL::Relocation{ RELOCATION_ID(15535, 15712) }.address();

#pragma pack(1)
	struct { BYTE db[5]; UINT8 jmp; INT32 rel32; } aob{
		{ 0x44, 0x89, 0x4C, 0x24, 0x20 }, 0xE9, 0 }; //jmp to pFunc + 5 

	if (memcmp(&aob, (void*)pfn, 5)) {
		switch (*(BYTE*)pfn) {
		case 0xFF: // abs long jump
		case 0xE8: // rel32 call
		case 0xE9: // rel32 jump
			char name[MAX_PATH] = {};
			if( uintptr_t offset = getModuleOffset(pfn, name, sizeof(name)))
				SKSE::stl::report_and_fail(std::format("AOB missmatch at [SkyrimSE.exe+{:X}]\n{}.\n\nTargeting\n{}+{:X}",
					pfn - (uintptr_t)GetModuleHandle(NULL), prnAOB(*(BYTE(*)[5])pfn), name, offset), SOURCE("plugin.cpp"));
		}
		SKSE::stl::report_and_fail(std::format("AOB missmatch at [SkyrimSE.exe+{:X}]\n{}.\n\nPossible unsupported binary or not compatible with other mod.",
			pfn - (uintptr_t)GetModuleHandle(NULL), prnAOB(*(BYTE(*)[5])pfn)), SOURCE("plugin.cpp"));
	}
	auto& trampoline = SKSE::GetTrampoline();

	if (AttachArmor_orig = trampoline.allocate(sizeof(aob)); !AttachArmor_orig)
		SKSE::stl::report_and_fail("Failed to allocate memory for AttachArmor hook", SOURCE("plugin.cpp"));

	aob.rel32 = (INT32)(pfn + sizeof(aob.db) - (uintptr_t)AttachArmor_orig - sizeof(aob));
	memcpy_s(AttachArmor_orig, sizeof(aob), &aob, sizeof(aob));

	trampoline.write_branch<5>(pfn, (uintptr_t)AttachArmor_hook);
}
void attachPlayer(RE::BSTSmartPointer<RE::BSScript::Object> script) {
	if (RE::BSScript::Variable* custom{}, * col{}, * hair{}; 
		(custom = script->GetVariable({ "_customHair"sv })) && custom->GetBool() &&
		(col = script->GetVariable({ "_color"sv })) && 
		(hair = script->GetVariable({"_hairColor"sv})))
	{
		// _hairColor form should always be avaliable when this is called.
		attach(RE::PlayerCharacter::GetSingleton()->Get3D(), hair->Unpack<RE::BGSColorForm*>());
	}
}