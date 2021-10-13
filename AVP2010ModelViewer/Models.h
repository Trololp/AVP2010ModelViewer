#pragma once
#include "MainClass_Viewer_Screen.h"
#pragma pack(push, 1)
struct model_header
{
	DWORD model_mesh_info_count;
	DWORD vertex_count;
	DWORD index_count;
	DWORD unk;
	DWORD count_unk_chunks;
	bool  vertex_layout; // 0 - TRIANGLESTRIP 1 - TRIANGLE LIST
};
#pragma pack(pop)

struct model_mat_info
{
	DWORD mat_hash;
	DWORD unk;
	DWORD points_count;
	DWORD some_flags; // 0 bit set is Meat? others is body chunks identification ? 
	DWORD unk2;
	DWORD unk3;
};

struct skeleton_bone
{
	DWORD parent_bone_id;
	DWORD unk_bone;
	char* name;
	XMFLOAT3 rel_pos;
	XMFLOAT4 rel_rot;
	XMMATRIX bone_local_matrix;
	XMMATRIX bone_model_matrix;
	XMMATRIX inv_bone_matrix;
};

struct model_skeleton
{
	char* model_name;
	DWORD model_hash;
	DWORD amount_bones;
	skeleton_bone* pBones;
	float magnify;
};

struct model_bone_rot
{
	XMFLOAT4 rot;
	float t_perc;
};

struct model_bone_anim
{
	DWORD frames;
	bool not_change_lenght;
	model_bone_rot* rots;
	XMFLOAT3* lens;
};

struct model_skeleton_anim
{
	DWORD type;
	char* anim_name;
	DWORD anim_hash;
	DWORD amount_bones;
	bool have_extra_bone_anim;
	model_bone_anim* bone_anims;
	float total_time;
};

struct model_info
{
	ID3D11Buffer *pVertexBuff;
	ID3D11Buffer *pIndexBuff;
	model_mat_info* mmi;
	UINT		mmi_count;
	UINT		vertex_layout_triangle_list;
	char*		mdl_name;
	int			empty_model;
	int			model_index_in_mii;
};

struct HMPT_entry
{
	XMFLOAT3 pos;
	float unk2;
	XMFLOAT4 q1;
	XMFLOAT4 q2;
	DWORD unk;
	char* name;
};

struct HMPT_info
{
	int count;
	HMPT_entry* e;
};

struct model_incremental_info
{
	char* model_name;
	model_info* mi;
	model_skeleton* skl;
	ID3D11Buffer* dynamic_skeleton_buffer;
	simple_vertex* dynamic_skeleton_points;
	ID3D11Buffer* static_HSBB_buffer;
	DWORD points_skeleton;
	DWORD points_HSBB;
	HMPT_info* HMPT_info = nullptr;
};

struct anim_incremental_info
{
	char* anim_name;
	DWORD bones_count;
	model_skeleton_anim* anim;
	float total_time;
	float current_anim_time;
	int total_frames;
	int current_frame;
};



int Read_models(const wchar_t* models_folder_path);
void Read_model(const wchar_t* model_name, int skip);
void Delete_models();
int Read_HSKNs(const wchar_t* MARE_folder_path);
void Update_Incremental_MMI_HSKN();
int Read_materials(const wchar_t* MARE_folder_path);
int Read_HANMs(const wchar_t* folder_path);
void Skeleton_setup_bone_anim_matrices();
int Model_anim_update_init(model_skeleton* skl, model_skeleton_anim* anim, float t);
int Model_anim_update(model_skeleton* skl, model_skeleton_anim* anim, float t, DWORD index, XMMATRIX* M_anim_prev);
//int Model_anim_update_with_frame(model_skeleton* skl, model_skeleton_anim* anim, int frame, DWORD index, XMMATRIX* M_prev);
//int Model_anim_update_with_frame_init(model_skeleton* skl, model_skeleton_anim* anim, int frame);
int Read_HSBBs(const wchar_t* folder_path);
int Read_HMPTs(const wchar_t* folder_path);
void Update_Incremental_MMI_HMPT();