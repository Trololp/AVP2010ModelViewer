// This is made only for one task! 
// Export model to .gltf file format
// gltf specifications -> https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html
//

#include "stdafx.h"
#include "Model_Exporter_gltf.h"
#include "Debug.h"
#include "Json.h"
#include "Models.h"
#include "Hash_tree.h"
#include <ShObjIdl.h>

// model vertex in avp2010 have this layout 
// "POSITION",    DXGI_FORMAT_R32G32B32_FLOAT,   aligned to 16 bytes 
// "BLENDINDICES" DXGI_FORMAT_R8G8B8A8_UINT,     
// "BLENDWEIGHT", DXGI_FORMAT_R16G16B16A16_SNORM,
// "TANGENT",     DXGI_FORMAT_R16G16B16A16_FLOAT,
// "BINORMAL",    DXGI_FORMAT_R16G16B16A16_FLOAT,
// "NORMAL",      DXGI_FORMAT_R16G16B16A16_FLOAT,
// "TEXCOORD0",   DXGI_FORMAT_R16G16B16A16_FLOAT,
// "TEXCOORD1",   DXGI_FORMAT_R32_FLOAT,         
//

		 



struct gltf_node_desc {
	char* name;
	XMVECTOR translation_v = g_XMZero;
	XMVECTOR rotation_v = g_XMIdentityR3;
	UINT parent_node = -1;
	UINT child_amt = 0;
	UINT* childs = nullptr;

	bool no_childs = true;
	bool no_parent = true;
};

struct gltf_material_info
{
	DWORD mat_hash;
	char* albedo_texture_name;
	UINT albedo_texture_index;
	char* normal_texture_name;
	UINT normal_texture_index;
	char* alpha_texture_name;
	UINT alpha_texture_index;
	char* specular_texture_name;
	UINT specular_texture_index;
	char* emission_texture_name;
	UINT emission_texture_index;
};

struct model_data_primitive
{
	UINT mat_id;

	// indeces
	UINT amount_indeces;
	short*      p_Indecies;
};



struct gltf_node_animation
{
	UINT node_id;

	bool is_not_use_translation;

	UINT amount_frames;
	float* key_frames; 

	XMFLOAT3* translations;
	XMFLOAT4* rotations;
};

struct gltf_animation
{
	char* name;

	UINT amount_nodes;
	gltf_node_animation* pNodeAnims;

	bool have_origin_anim;
	gltf_node_animation* pOriginNodeAnim;
};

struct model_data
{
	char* model_name;
	UINT model_id; // in mmi
	bool vertex_layout_is_triangle_list;
	UINT mmi_count;
	model_mat_info* mmi;

	UINT amount_materials;
	gltf_material_info* materials;

	UINT amount_primitives;
	model_data_primitive* primitives;

	UINT* valid_primitives_id;

	// nodes are bones + 1 origin node
	UINT amount_nodes;
	gltf_node_desc* nodes;
	XMMATRIX* p_inverseBindMatrices;

	// vertex data
	UINT amount_vertex;

	XMFLOAT3*    p_Pos;
	BYTE*      p_Blendids;
	XMFLOAT4*   p_BlendWeights;
	XMFLOAT3*   p_Tangent;
	XMFLOAT3*   p_Binormal;
	XMFLOAT3*   p_Normal;
	XMFLOAT2*   p_UV;
	float*      p_Unknown;
	
	bool bHaveAnimations;
	UINT amount_animations;
	gltf_animation* pAnims;
};



struct gltf_bufferview_desc {
	UINT buff_id;
	UINT byte_lenght = 0;
	UINT byte_offset = 0;
	UINT byte_stride = 0;
};

enum gltf_component_type
{
	GL_BYTE = 5120,
	GL_UNSIGNED_BYTE, // 5121
	GL_SHORT, //5122
	GL_UNSIGNED_SHORT, //5123
	GL_INT, // 5124
	GL_UNSGNED_INT,// 5125
	GL_FLOAT, // 5126

	GL_DOUBLE = 5130// 5130
};

enum gltf_accessor_type
{
	GL_SCALAR,
	GL_VEC2,
	GL_VEC3,
	GL_VEC4,
	GL_MAT2,
	GL_MAT3,
	GL_MAT4
};

struct gltf_accessor_desc {
	UINT bufferview_id;
	UINT byte_offset;
	UINT count;
	gltf_accessor_type type;
	gltf_component_type component_type;
};

extern model_incremental_info g_mmi[1000];
extern anim_incremental_info g_anm[2000];
extern DWORD g_loaded_anim_count;
extern DWORD g_selected_anim_index;
extern avp_texture AVP_TEXTURES[1500];
extern wchar_t* g_path1;
extern int read_padded_str(HANDLE file, char* dest);


model_data* g_exporting_model_data;

UINT g_accessors_amount = 0;
gltf_accessor_desc* g_accessors;
UINT g_buffer_views_amount = 0;
gltf_bufferview_desc* g_bufferviews;
UINT g_texture_amt = 0;
UINT g_animation_start_accessor_id = 0;

json_parser g_JP;

UINT g_joints_hack_arr[200] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
	17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
	31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
	45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58,
	59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72,
	73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86,
	87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100,
	101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
	112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122,
	123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133,
	134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144,
	145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155,
	156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166,
	167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177,
	178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188,
	189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200 };

bool Fill_material_info(char* User_selected_path)
{
	std::vector<DWORD> used_hashs;
	std::vector<gltf_material_info> gltf_materials;


	char material_name[200] = { 0 };
	UINT material_id = 0;


	for (UINT i = 0; i < g_exporting_model_data->mmi_count; i++)
	{
		DWORD mat_hash = g_exporting_model_data->mmi[i].mat_hash;
		if (g_exporting_model_data->mmi[i].some_flags & 1) // no need meat chunks
		{
			g_exporting_model_data->primitives[i].mat_id = material_id;
			continue;
		}
			

		if (std::find(used_hashs.begin(), used_hashs.end(), mat_hash) != used_hashs.end())
		{
			UINT prev_mat_id = 0;

			for (UINT j = 0; j < used_hashs.size(); j++)
			{
				if (mat_hash == used_hashs[j])
				{
					prev_mat_id = j;
					break;
				}
			}


			g_exporting_model_data->primitives[i].mat_id = prev_mat_id; // so it is previous 
			continue;
		}
		else
		{
			used_hashs.push_back(mat_hash);
			gltf_material_info curr_mat;
			ZeroMemory(&curr_mat, sizeof(gltf_material_info));

			curr_mat.mat_hash = mat_hash;
			g_exporting_model_data->primitives[i].mat_id = material_id;
			material_id++;

			list_node* ln;
			if ((ln = search_mat_by_hash(mat_hash)))
			{
				if (ln->material->texture_diff_id != -1)
				{

					wchar_t* file_name = wcsrchr(AVP_TEXTURES[ln->material->texture_diff_id].path, L'\\');
					char* file_name2 = (char*)malloc(MAX_PATH);
					ZeroMemory(file_name2, MAX_PATH);
					wcstombs(file_name2, file_name + 1, MAX_PATH);
					curr_mat.albedo_texture_name = file_name2;

					wchar_t File_dest_dir[260];
					wsprintf(File_dest_dir, L"%S\\%S", User_selected_path, file_name2);
					CopyFile(AVP_TEXTURES[ln->material->texture_diff_id].path, File_dest_dir, NULL);

					if (GetLastError())
					{
						dbgprint(__FUNCTION__, "CopyFile Error: %d (%ws to %ws) \n", GetLastError(), AVP_TEXTURES[ln->material->texture_diff_id].path, File_dest_dir);
					}
				}

				if (ln->material->texture_norm_id != -1)
				{
					wchar_t* file_name = wcsrchr(AVP_TEXTURES[ln->material->texture_norm_id].path, L'\\');
					char* file_name2 = (char*)malloc(MAX_PATH);
					ZeroMemory(file_name2, MAX_PATH);
					wcstombs(file_name2, file_name + 1, MAX_PATH);
					curr_mat.normal_texture_name = file_name2;

					wchar_t File_dest_dir[260];
					wsprintf(File_dest_dir, L"%S\\%S", User_selected_path, file_name2);
					CopyFile(AVP_TEXTURES[ln->material->texture_norm_id].path, File_dest_dir, NULL);

					if (GetLastError())
					{
						dbgprint(__FUNCTION__, "CopyFile Error: %d (%ws to %ws) \n", GetLastError(), AVP_TEXTURES[ln->material->texture_norm_id].path, File_dest_dir);
					}
				}

				if (ln->material->texture_spec_id != -1)
				{
					wchar_t* file_name = wcsrchr(AVP_TEXTURES[ln->material->texture_spec_id].path, L'\\');
					char* file_name2 = (char*)malloc(MAX_PATH);
					ZeroMemory(file_name2, MAX_PATH);
					wcstombs(file_name2, file_name + 1, MAX_PATH);
					curr_mat.specular_texture_name = file_name2;

					wchar_t File_dest_dir[260];
					wsprintf(File_dest_dir, L"%S\\%S", User_selected_path, file_name2);
					CopyFile(AVP_TEXTURES[ln->material->texture_spec_id].path, File_dest_dir, NULL);

					if (GetLastError())
					{
						dbgprint(__FUNCTION__, "CopyFile Error: %d (%ws to %ws) \n", GetLastError(), AVP_TEXTURES[ln->material->texture_norm_id].path, File_dest_dir);
					}
				}

				if (ln->material->texture_emission_id != -1)
				{
					wchar_t* file_name = wcsrchr(AVP_TEXTURES[ln->material->texture_emission_id].path, L'\\');
					char* file_name2 = (char*)malloc(MAX_PATH);
					ZeroMemory(file_name2, MAX_PATH);
					wcstombs(file_name2, file_name + 1, MAX_PATH);
					curr_mat.emission_texture_name = file_name2;

					wchar_t File_dest_dir[260];
					wsprintf(File_dest_dir, L"%S\\%S", User_selected_path, file_name2);
					CopyFile(AVP_TEXTURES[ln->material->texture_emission_id].path, File_dest_dir, NULL);

					if (GetLastError())
					{
						dbgprint(__FUNCTION__, "CopyFile Error: %d (%ws to %ws) \n", GetLastError(), AVP_TEXTURES[ln->material->texture_norm_id].path, File_dest_dir);
					}
				}
				
				if (ln->material->texture_alpha_id != -1)
				{
					wchar_t* file_name = wcsrchr(AVP_TEXTURES[ln->material->texture_alpha_id].path, L'\\');
					char* file_name2 = (char*)malloc(MAX_PATH);
					ZeroMemory(file_name2, MAX_PATH);
					wcstombs(file_name2, file_name + 1, MAX_PATH);
					curr_mat.alpha_texture_name = file_name2;

					wchar_t File_dest_dir[260];
					wsprintf(File_dest_dir, L"%S\\%S", User_selected_path, file_name2);
					CopyFile(AVP_TEXTURES[ln->material->texture_alpha_id].path, File_dest_dir, NULL);

					if (GetLastError())
					{
						dbgprint(__FUNCTION__, "CopyFile Error: %d (%ws to %ws) \n", GetLastError(), AVP_TEXTURES[ln->material->texture_norm_id].path, File_dest_dir);
					}
				}

			}
			else
			{
				// default texture
				curr_mat.albedo_texture_name = nullptr;
				curr_mat.normal_texture_name = nullptr;
				curr_mat.alpha_texture_name = nullptr;
				curr_mat.emission_texture_name = nullptr;
				curr_mat.specular_texture_name = nullptr;
			}

			gltf_materials.push_back(curr_mat);

		}
	}

	gltf_material_info* gltf_mat_buffer = (gltf_material_info*)malloc(sizeof(gltf_material_info) * gltf_materials.size());
	ZeroMemory(gltf_mat_buffer, sizeof(gltf_material_info) * gltf_materials.size());

	memcpy(gltf_mat_buffer, gltf_materials.data(), gltf_materials.size()*sizeof(gltf_material_info));

	g_exporting_model_data->materials = gltf_mat_buffer;
	g_exporting_model_data->amount_materials = gltf_materials.size();

	return true;
}

bool Read_File_and_fill_vertex_data(wchar_t* file_path)
{
	DWORD a = 0;

	HANDLE f = CreateFileW(file_path, GENERIC_READ, NULL, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (GetLastError() != ERROR_SUCCESS)
	{
		dbgprint("Export", "Opening file Error: %d %s\n", GetLastError(), file_path);
		return false;
	}

	struct asura_header {
		DWORD magic;
		DWORD len_chunk;
		DWORD ver0;
		DWORD ver1;
		DWORD RSCF_type;
		DWORD RSCF_subtype;
		DWORD len_resource;
	};

	asura_header hdr;
	READ(hdr);

	//DWORD* asura_header = (DWORD*)malloc(28);
	//ReadFile(file, asura_header, 28, &bytes_readen, NULL);

	//DWORD hdr_version = hdr.ver0;//*(asura_header + 2);

	if (hdr.magic != (DWORD)'FCSR' || hdr.RSCF_subtype != (DWORD)0xF)
	{
		dbgprint(__FUNCTION__, "Opening file Error: BAD_HEADER (HEADER: %08x Type %02d)\n", hdr.magic, hdr.RSCF_subtype);
		CloseHandle(f);
		return false;
	}

	char* name_str = (char*)malloc(260);
	read_padded_str(f, name_str);
	//dbgprint("Model", "RSCF package name: %s \n", name_str);
	model_header mdl_header;
	model_data mdl_data;

	if (READ(mdl_header))
	{
		//dbgprint("Models", "Model header: mmi_count: %d, Vertexes: %d, Indexes: %d \n", mdl_header.model_mesh_info_count, mdl_header.vertex_count, mdl_header.index_count);
		if (mdl_header.model_mesh_info_count == 0)
		{
			dbgprint("Model", "Empty model \n");
			return false;
		}


		mdl_data.vertex_layout_is_triangle_list = mdl_header.vertex_layout;
		mdl_data.mmi_count = mdl_header.model_mesh_info_count;

		model_mat_info* mmi_buff = (model_mat_info*)_aligned_malloc(mdl_header.model_mesh_info_count * sizeof(model_mat_info), 16);
		if (!READP(mmi_buff, mdl_header.model_mesh_info_count * sizeof(model_mat_info)))
		{
			dbgprint("Models", "Read mmi error: %d \n", GetLastError());
			free(name_str);
			CloseHandle(f);
			return false;
		}

		mdl_data.mmi = mmi_buff;

		model_vtx* vertex_buff = (model_vtx*)_aligned_malloc(mdl_header.vertex_count * sizeof(model_vtx), 16);

		if (!READP(vertex_buff, mdl_header.vertex_count * sizeof(model_vtx)))
		{
			dbgprint("Models", "Read vertex error: %d \n", GetLastError());
			free(name_str);
			CloseHandle(f);
			return false;
		}

		// Sort vertex to different buffers
		UINT cnt = mdl_header.vertex_count;

		mdl_data.p_Pos =          (XMFLOAT3*)malloc(sizeof(XMFLOAT3)*cnt);
		mdl_data.p_Blendids =     (BYTE*)malloc(sizeof(BYTE)*cnt*4); // 4 ids per vertex
		mdl_data.p_BlendWeights = (XMFLOAT4*)malloc(sizeof(XMFLOAT4)*cnt);
		mdl_data.p_Normal =       (XMFLOAT3*)malloc(sizeof(XMFLOAT3)*cnt);
		mdl_data.p_Binormal =     (XMFLOAT3*)malloc(sizeof(XMFLOAT3)*cnt);
		mdl_data.p_Tangent =      (XMFLOAT3*)malloc(sizeof(XMFLOAT3)*cnt);
		mdl_data.p_UV =           (XMFLOAT2*)malloc(sizeof(XMFLOAT2)*cnt);
		mdl_data.p_Unknown =      (float*)malloc(sizeof(float)*cnt);

		// this is messy
		for (UINT i = 0; i < mdl_header.vertex_count; i++)
		{
			mdl_data.p_Pos[i] = { vertex_buff[i].Pos.x, vertex_buff[i].Pos.y, vertex_buff[i].Pos.z}; // negative ?
			mdl_data.p_Blendids[4 * i + 0] = vertex_buff[i].BlendIndices[0];
			mdl_data.p_Blendids[4 * i + 1] = vertex_buff[i].BlendIndices[1];
			mdl_data.p_Blendids[4 * i + 2] = vertex_buff[i].BlendIndices[2];
			mdl_data.p_Blendids[4 * i + 3] = vertex_buff[i].BlendIndices[3];
			mdl_data.p_BlendWeights[i] = { ((float)vertex_buff[i].BlendWeight[0] / 32767.0f) ,
										   ((float)vertex_buff[i].BlendWeight[1] / 32767.0f) ,
				                           ((float)vertex_buff[i].BlendWeight[2] / 32767.0f) ,
				                           ((float)vertex_buff[i].BlendWeight[3] / 32767.0f)  };
			mdl_data.p_Normal[i] = { (float)vertex_buff[i].Normal[0], (float)vertex_buff[i].Normal[1] , (float)vertex_buff[i].Normal[2] }; // negative ?
			mdl_data.p_Binormal[i] = { (float)vertex_buff[i].Binormal[0], (float)vertex_buff[i].Binormal[1] , (float)vertex_buff[i].Binormal[2] };
			mdl_data.p_Tangent[i] = { (float)vertex_buff[i].Tangent[0], (float)vertex_buff[i].Tangent[1] , (float)vertex_buff[i].Tangent[2] };
			mdl_data.p_UV[i] = { (float)vertex_buff[i].TexCoord1[0], (float)vertex_buff[i].TexCoord1[1] };
			mdl_data.p_Unknown[i] = vertex_buff[i].TexCoord2;
		}

		USHORT* index_buff = (USHORT*)_aligned_malloc(mdl_header.index_count * sizeof(USHORT), 16);

		if (!READP(index_buff, mdl_header.index_count * sizeof(USHORT)))
		{
			dbgprint("Models", "Read indexes error: %d \n", GetLastError());
			free(name_str);
			CloseHandle(f);
			return false;
		}

		model_data_primitive* primitives = (model_data_primitive*)malloc(sizeof(model_data_primitive)*mdl_data.mmi_count);
		mdl_data.amount_primitives = mdl_data.mmi_count;
		mdl_data.primitives = primitives;
		UINT offset = 0;

		for (UINT i = 0; i < mdl_data.amount_primitives; i++)
		{
			// setup different indices buffers
			primitives[i].amount_indeces = mdl_data.mmi[i].points_count;
			short* indecies_buff = (short*)malloc(sizeof(short)*primitives[i].amount_indeces);
			memcpy(indecies_buff, index_buff + offset, sizeof(short)*primitives[i].amount_indeces);
			primitives[i].p_Indecies = indecies_buff;

			offset += primitives[i].amount_indeces;
		}
		
		if (offset != mdl_header.index_count)
		{
			dbgprint(__FUNCTION__, "total indeces copied != header.index_count \n");
			free(name_str);
			CloseHandle(f);
			return false;
		}

		g_exporting_model_data = new model_data;

		//vtx data
		g_exporting_model_data->amount_vertex = mdl_header.vertex_count;

		g_exporting_model_data->p_Pos = mdl_data.p_Pos;
		g_exporting_model_data->p_Blendids = mdl_data.p_Blendids;
		g_exporting_model_data->p_BlendWeights = mdl_data.p_BlendWeights;
		g_exporting_model_data->p_Tangent = mdl_data.p_Tangent;
		g_exporting_model_data->p_Binormal = mdl_data.p_Binormal;
		g_exporting_model_data->p_Normal = mdl_data.p_Normal;
		g_exporting_model_data->p_UV = mdl_data.p_UV;
		g_exporting_model_data->p_Unknown = mdl_data.p_Unknown;

		// sub meshes
		g_exporting_model_data->amount_primitives = mdl_data.amount_primitives;
		g_exporting_model_data->primitives = mdl_data.primitives;

		// sub mesh info
		g_exporting_model_data->mmi = mdl_data.mmi;
		g_exporting_model_data->mmi_count = mdl_data.mmi_count;

		g_exporting_model_data->vertex_layout_is_triangle_list = mdl_data.vertex_layout_is_triangle_list;

		g_exporting_model_data->model_name = name_str;


	}

	CloseHandle(f);
	return true;
}

bool Fill_nodes_info()
{
	model_skeleton* skl = g_mmi[g_exporting_model_data->model_id].skl;

	
	if (skl)
		g_exporting_model_data->amount_nodes = skl->amount_bones + 1;
	else
		g_exporting_model_data->amount_nodes = 1;

	if (!skl)
	{
		gltf_node_desc* p_nodes = (gltf_node_desc*)malloc(sizeof(gltf_node_desc));
		p_nodes[0].name = g_mmi[g_exporting_model_data->model_id].model_name;
		p_nodes[0].translation_v = g_XMZero;
		p_nodes[0].rotation_v = g_XMIdentityR3;
		p_nodes[0].parent_node = -1;
		p_nodes[0].child_amt = 0;
		p_nodes[0].no_childs = true;
		p_nodes[0].no_parent = true;

		g_exporting_model_data->nodes = p_nodes;
		g_exporting_model_data->p_inverseBindMatrices = nullptr;

		return true;
	}

	gltf_node_desc* p_nodes = (gltf_node_desc*)malloc(sizeof(gltf_node_desc)*g_exporting_model_data->amount_nodes);
	XMMATRIX* p_matrices = (XMMATRIX*)malloc(sizeof(XMMATRIX)*skl->amount_bones);

	p_nodes[0].name = g_mmi[g_exporting_model_data->model_id].model_name;
	p_nodes[0].translation_v = g_XMZero;
	p_nodes[0].rotation_v = g_XMIdentityR3;
	p_nodes[0].parent_node = -1;
	p_nodes[0].child_amt = 1;
	UINT* node0_child = (UINT*)malloc(sizeof(UINT));
	node0_child[0] = 1;
	p_nodes[0].childs = node0_child;
	p_nodes[0].no_childs = false;
	p_nodes[0].no_parent = true;

	skeleton_bone* p_bones = skl->pBones;

	for (UINT i = 1; i < skl->amount_bones + 1; i++)
	{
		p_nodes[i].name = p_bones[i - 1].name;
		XMFLOAT3 t_vec = p_bones[i - 1].rel_pos;
		p_nodes[i].translation_v = XMLoadFloat3(&t_vec);

		XMFLOAT4 r_vec = p_bones[i - 1].rel_rot;
		p_nodes[i].rotation_v = XMLoadFloat4(&r_vec);

		p_nodes[i].parent_node = p_bones[i - 1].parent_bone_id + 1;

		p_nodes[i].no_parent = false;

		p_matrices[i - 1] = p_bones[i - 1].inv_bone_matrix;

	}

	// Fill child info by parent info
	for (UINT i = 1; i < skl->amount_bones + 1; i++)
	{
		std::vector<UINT> childs;
		for (UINT j = i + 1; j < skl->amount_bones + 1; j++)
		{
			if (i == p_nodes[j].parent_node)
				childs.push_back(j);
		}

		if (childs.size() > 0)
		{
			p_nodes[i].no_childs = false;
			p_nodes[i].child_amt = childs.size();
		}
		else
		{
			p_nodes[i].no_childs = true;
			p_nodes[i].child_amt = 0;
			continue;
		}

		UINT* p_childs = (UINT*)malloc(sizeof(UINT) * childs.size());
		memcpy(p_childs, childs.data(), sizeof(UINT) * childs.size());

		p_nodes[i].childs = p_childs;

	}

	g_exporting_model_data->nodes = p_nodes;
	g_exporting_model_data->p_inverseBindMatrices = p_matrices;

	return true;
}

bool fill_animations_info(bool only_current_animation)
{
	model_skeleton* skl = g_mmi[g_exporting_model_data->model_id].skl;
	
	if (!skl)
	{
		g_exporting_model_data->bHaveAnimations = false;
		return true;
	}

	if (skl->amount_bones <= 1) // there alot strange animations with props like 360 rotating animation etc.
	{
		g_exporting_model_data->bHaveAnimations = false;
		return true;
	}

	std::vector<UINT> supported_anims;

	UINT amt_bones_required = skl->amount_bones;
	if (only_current_animation)
	{
		if(amt_bones_required == g_anm[g_selected_anim_index].bones_count)
			supported_anims.push_back(g_selected_anim_index);
	}
	else
	{
		for (int i = 0; i < g_loaded_anim_count; i++)
			if (amt_bones_required == g_anm[i].bones_count)
				supported_anims.push_back(i);
	}


	if (supported_anims.size() == 0)
	{
		g_exporting_model_data->bHaveAnimations = false;
		return true;
	}

	gltf_animation* pAnims = (gltf_animation*)malloc(sizeof(gltf_animation) * supported_anims.size());

	UINT i = 0;

	for (UINT anim_id : supported_anims)
	{
		pAnims[i].name = g_anm[anim_id].anim_name;

		pAnims[i].amount_nodes = amt_bones_required;

		pAnims[i].have_origin_anim = g_anm[anim_id].anim->have_extra_bone_anim;


		gltf_node_animation* pNodeAnims = (gltf_node_animation*)malloc(sizeof(gltf_node_animation) * pAnims[i].amount_nodes);
		model_bone_anim* bone_anims = g_anm[anim_id].anim->bone_anims;

		if (pAnims[i].have_origin_anim)
		{
			gltf_node_animation* pNodeOriginAnim = (gltf_node_animation*)malloc(sizeof(gltf_node_animation) * 1);
			pNodeOriginAnim->amount_frames = bone_anims[amt_bones_required].frames;
			pNodeOriginAnim->is_not_use_translation = bone_anims[amt_bones_required].not_change_lenght;

			float* key_frames = (float*)malloc(sizeof(float)* pNodeOriginAnim->amount_frames);
			XMFLOAT4* rotations = (XMFLOAT4*)malloc(sizeof(XMFLOAT4)* pNodeOriginAnim->amount_frames);
			XMFLOAT3* translations = nullptr;

			if (!pNodeOriginAnim->is_not_use_translation)
			{
				translations = (XMFLOAT3*)malloc(sizeof(XMFLOAT3)* pNodeOriginAnim->amount_frames);
			}

			if (pNodeOriginAnim->is_not_use_translation)
			{
				for (UINT k = 0; k < pNodeOriginAnim->amount_frames; k++)
				{
					key_frames[k] = bone_anims[amt_bones_required].rots[k].t_perc * g_anm[anim_id].anim->total_time;
					rotations[k] = bone_anims[amt_bones_required].rots[k].rot;
				}
			}
			else
			{
				for (UINT k = 0; k < pNodeOriginAnim->amount_frames; k++)
				{
					key_frames[k] = bone_anims[amt_bones_required].rots[k].t_perc * g_anm[anim_id].anim->total_time;
					rotations[k] = bone_anims[amt_bones_required].rots[k].rot;
					translations[k] = bone_anims[amt_bones_required].lens[k];
				}
			}

			pNodeOriginAnim->key_frames = key_frames;
			pNodeOriginAnim->rotations = rotations;
			pNodeOriginAnim->translations = translations;

			pAnims[i].pOriginNodeAnim = pNodeOriginAnim;

		}
		else
		{
			pAnims[i].pOriginNodeAnim = nullptr;
		}


		for (UINT j = 0; j < pAnims[i].amount_nodes; j++)
		{
			pNodeAnims[j].node_id = j + 1;
			pNodeAnims[j].amount_frames = bone_anims[j].frames;
			pNodeAnims[j].is_not_use_translation = bone_anims[j].not_change_lenght;

			float* key_frames = (float*)malloc(sizeof(float)* pNodeAnims[j].amount_frames);
			XMFLOAT4* rotations = (XMFLOAT4*)malloc(sizeof(XMFLOAT4)* pNodeAnims[j].amount_frames);
			XMFLOAT3* translations = nullptr;

			if (!pNodeAnims[j].is_not_use_translation)
			{
				translations = (XMFLOAT3*)malloc(sizeof(XMFLOAT3)* pNodeAnims[j].amount_frames);
			}

			if (pNodeAnims[j].is_not_use_translation)
			{
				for (UINT k = 0; k < pNodeAnims[j].amount_frames; k++)
				{
					key_frames[k] = bone_anims[j].rots[k].t_perc * g_anm[anim_id].anim->total_time;
					rotations[k] = bone_anims[j].rots[k].rot;
				}
			}
			else
			{
				for (UINT k = 0; k < pNodeAnims[j].amount_frames; k++)
				{
					key_frames[k] = bone_anims[j].rots[k].t_perc * g_anm[anim_id].anim->total_time;
					rotations[k] = bone_anims[j].rots[k].rot;
					translations[k] = bone_anims[j].lens[k];
				}
			}

			pNodeAnims[j].key_frames = key_frames;
			pNodeAnims[j].rotations = rotations;
			pNodeAnims[j].translations = translations;

		}

		pAnims[i].pNodeAnims = pNodeAnims;
		i++;
	}

	g_exporting_model_data->bHaveAnimations = true;
	g_exporting_model_data->pAnims = pAnims;
	g_exporting_model_data->amount_animations = supported_anims.size();

	return true;
}

UINT get_size_of_GL_component_type(gltf_component_type type)
{
	switch (type)
	{
	case GL_BYTE:
	case GL_UNSIGNED_BYTE:
		return 1;
	case GL_SHORT:
	case GL_UNSIGNED_SHORT:
		return 2;
	case GL_INT:
	case GL_UNSGNED_INT:
	case GL_FLOAT:
		return 4;
	case GL_DOUBLE:
		return 8;
	default:
		dbgprint("get_size_of_GL_type", "unknown GL component type %d", type);
		return 0;
	}
}

UINT get_size_of_GL_type(gltf_accessor_type type, gltf_component_type comp_type)
{
	UINT comp_size = get_size_of_GL_component_type(comp_type);

	switch (type)
	{
	case GL_SCALAR:
		return comp_size;
	case GL_VEC2:
		return 2 * comp_size;
	case GL_VEC3:
		return 3 * comp_size;
	case GL_VEC4:
		return 4 * comp_size;
	case GL_MAT2:
		if (comp_size == 1) // data aligment 
			return 8;
		return 4 * comp_size;
	case GL_MAT3:
		if (comp_size == 1)
			return 12;
		if (comp_size == 2)
			return 24;
		return 9 * comp_size;
	case GL_MAT4:
		return 16 * comp_size;
	default:
		dbgprint("get_amt_elems_from_GL_type", "unknown GL type %d", type);
		return 0;
	}
}

bool Fill_data_file(char* file_path, UINT* pData_len)
{
	DWORD a = 0;

	HANDLE f2 = CreateFileA(file_path, GENERIC_WRITE, NULL, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (GetLastError() != ERROR_SUCCESS && GetLastError() != ERROR_ALREADY_EXISTS)
	{
		dbgprint("Export", "Creating file Error: %d %s\n", GetLastError(), file_path);
		return false;
	}

	UINT no_meat_primitives_cnt = 0;
	std::vector<UINT> no_meat_primitives;


	for (UINT i = 0; i < g_exporting_model_data->mmi_count; i++)
	{
		if (g_exporting_model_data->mmi[i].some_flags & 1) // meat primitive
		{
			continue;
		}
		no_meat_primitives_cnt++;
		no_meat_primitives.push_back(i);
	}

	g_exporting_model_data->amount_primitives = no_meat_primitives_cnt;
	UINT* p_valid_primitives_id = (UINT*)malloc(sizeof(UINT)*no_meat_primitives_cnt);
	memcpy(p_valid_primitives_id, no_meat_primitives.data(), sizeof(UINT)*no_meat_primitives_cnt);

	g_exporting_model_data->valid_primitives_id = p_valid_primitives_id;

	UINT animation_accessors = 0;

	if (g_exporting_model_data->bHaveAnimations)
	{
		gltf_animation* pAnims = g_exporting_model_data->pAnims;

		for (int i = 0; i < g_exporting_model_data->amount_animations; i++)
		{
			for (int j = 0; j < pAnims[i].amount_nodes; j++)
			{
				animation_accessors += pAnims[i].pNodeAnims[j].is_not_use_translation ? 2 : 3;
			}

			if (pAnims[i].have_origin_anim)
			{
				animation_accessors += pAnims[i].pOriginNodeAnim->is_not_use_translation ? 2 : 3;
			}
		}
	}

	UINT amt_accessors = 8 + no_meat_primitives_cnt; //animation_accessors;
	g_animation_start_accessor_id = amt_accessors;

	g_buffer_views_amount = (amt_accessors + animation_accessors);
	g_accessors_amount = (amt_accessors + animation_accessors);

	g_bufferviews = (gltf_bufferview_desc*)malloc(sizeof(gltf_bufferview_desc)*(amt_accessors + animation_accessors));
	ZeroMemory(g_bufferviews, sizeof(gltf_bufferview_desc)*(amt_accessors + animation_accessors));

	g_accessors = (gltf_accessor_desc*)malloc(sizeof(gltf_accessor_desc)*(amt_accessors + animation_accessors));
	ZeroMemory(g_accessors, sizeof(gltf_accessor_desc)*(amt_accessors + animation_accessors));

	// in .gltf buffer they will be this layout
	// Signature ["This file was created ..."] 32b	 -	   chars
	// --------------------------------------------------------------------
	// InverseBindMatrices   amt_bones*64b			 0	   MAT4       float
	// Position              amt_vertex*12b			 1	   VEC3		  float
	// Joints   			 amt_vertex*4*1b		 2	   VEC4   	  unsigned byte
	// Weights				 amt_vertex*4*4b		 3	   VEC4   	  float
	// Normal				 amt_vertex*12b			 4	   VEC3   	  float
	// Binormal				 amt_vertex*12b			 5	   VEC3   	  float
	// Tangent				 amt_vertex*12b			 6	   VEC3   	  float
	// Texcoord				 amt_vertex*8b			 7	   VEC2   	  float
	// Indeces0				 amt_indeces0*2b		 8	   SCALAR     unsigned short
	// Indeces1				 ...					 ...
	// ...											 
	// IndecesN								         N+8   
	// --------------------------------------------------------------------
	// Anim0_node0_key_frames     amt_frames*4b      N+9+0   SCALAR   float
	// Anim0_node0_rotations      amt_frames*16b     N+9+1   VEC4     float
	// [Anim0_node0_translations] amt_frames*12b     N+9+2   VEC3     float

	g_accessors[0].type = GL_MAT4; g_accessors[0].component_type = GL_FLOAT;
	g_accessors[1].type = GL_VEC3; g_accessors[1].component_type = GL_FLOAT;
	g_accessors[2].type = GL_VEC4; g_accessors[2].component_type = GL_UNSIGNED_BYTE;
	g_accessors[3].type = GL_VEC4; g_accessors[3].component_type = GL_FLOAT;
	g_accessors[4].type = GL_VEC3; g_accessors[4].component_type = GL_FLOAT;
	g_accessors[5].type = GL_VEC3; g_accessors[5].component_type = GL_FLOAT;
	g_accessors[6].type = GL_VEC3; g_accessors[6].component_type = GL_FLOAT;
	g_accessors[7].type = GL_VEC2; g_accessors[7].component_type = GL_FLOAT;

	g_accessors[0].bufferview_id = 0; g_accessors[0].count = g_exporting_model_data->amount_nodes - 1;
	g_accessors[1].bufferview_id = 1; g_accessors[1].count = g_exporting_model_data->amount_vertex;
	g_accessors[2].bufferview_id = 2; g_accessors[2].count = g_exporting_model_data->amount_vertex;
	g_accessors[3].bufferview_id = 3; g_accessors[3].count = g_exporting_model_data->amount_vertex;
	g_accessors[4].bufferview_id = 4; g_accessors[4].count = g_exporting_model_data->amount_vertex;
	g_accessors[5].bufferview_id = 5; g_accessors[5].count = g_exporting_model_data->amount_vertex;
	g_accessors[6].bufferview_id = 6; g_accessors[6].count = g_exporting_model_data->amount_vertex;
	g_accessors[7].bufferview_id = 7; g_accessors[7].count = g_exporting_model_data->amount_vertex;

	// indeces
	for (UINT i = 8; i < amt_accessors; i++)
	{
		g_accessors[i].type = GL_SCALAR; g_accessors[i].component_type = GL_UNSIGNED_SHORT;
		g_accessors[i].bufferview_id = i; g_accessors[i].count = g_exporting_model_data->primitives[no_meat_primitives[i-8]].amount_indeces;
	}

	UINT anim_accessor_id = amt_accessors;

	if (g_exporting_model_data->bHaveAnimations)
	{
		gltf_animation* pAnims = g_exporting_model_data->pAnims;

		for (int i = 0; i < g_exporting_model_data->amount_animations; i++)
		{
			for (int j = 0; j < pAnims[i].amount_nodes; j++)
			{
				// keyframes
				g_accessors[anim_accessor_id].type = GL_SCALAR; g_accessors[anim_accessor_id].component_type = GL_FLOAT;
				g_accessors[anim_accessor_id].count = pAnims[i].pNodeAnims[j].amount_frames;
				g_accessors[anim_accessor_id].bufferview_id = anim_accessor_id;

				anim_accessor_id++;
				// rotations
				g_accessors[anim_accessor_id].type = GL_VEC4; g_accessors[anim_accessor_id].component_type = GL_FLOAT;
				g_accessors[anim_accessor_id].count = pAnims[i].pNodeAnims[j].amount_frames;
				g_accessors[anim_accessor_id].bufferview_id = anim_accessor_id;

				anim_accessor_id++;

				if (!pAnims[i].pNodeAnims[j].is_not_use_translation)
				{
					g_accessors[anim_accessor_id].type = GL_VEC3; g_accessors[anim_accessor_id].component_type = GL_FLOAT;
					g_accessors[anim_accessor_id].count = pAnims[i].pNodeAnims[j].amount_frames;
					g_accessors[anim_accessor_id].bufferview_id = anim_accessor_id;
					anim_accessor_id++;
				}

			}

			if (pAnims[i].have_origin_anim)
			{
				// keyframes
				g_accessors[anim_accessor_id].type = GL_SCALAR; g_accessors[anim_accessor_id].component_type = GL_FLOAT;
				g_accessors[anim_accessor_id].count = pAnims[i].pOriginNodeAnim->amount_frames;
				g_accessors[anim_accessor_id].bufferview_id = anim_accessor_id;

				anim_accessor_id++;
				// rotations
				g_accessors[anim_accessor_id].type = GL_VEC4; g_accessors[anim_accessor_id].component_type = GL_FLOAT;
				g_accessors[anim_accessor_id].count = pAnims[i].pOriginNodeAnim->amount_frames;
				g_accessors[anim_accessor_id].bufferview_id = anim_accessor_id;

				anim_accessor_id++;

				if (!pAnims[i].pOriginNodeAnim->is_not_use_translation)
				{
					g_accessors[anim_accessor_id].type = GL_VEC3; g_accessors[anim_accessor_id].component_type = GL_FLOAT;
					g_accessors[anim_accessor_id].count = pAnims[i].pOriginNodeAnim->amount_frames;
					g_accessors[anim_accessor_id].bufferview_id = anim_accessor_id;
					anim_accessor_id++;
				}
			}
		}
	}

	UINT offset = 32;
	
	for (UINT i = 0; i < amt_accessors + animation_accessors; i++)
	{
		UINT single_elem_size = get_size_of_GL_type(g_accessors[i].type, g_accessors[i].component_type);

		g_bufferviews[i].byte_lenght = g_accessors[i].count * single_elem_size;
		g_bufferviews[i].byte_offset = offset;
		offset += g_bufferviews[i].byte_lenght;
	}


	char Signature[32] = "Made by AVP2010ModelViewer     ";
	WRITEP(Signature, 32);

	// Matrices
	if (g_bufferviews[0].byte_lenght != 0)
	{
		WRITEP(g_exporting_model_data->p_inverseBindMatrices, g_bufferviews[0].byte_lenght);
	}

	

	// vertex position 
	WRITEP(g_exporting_model_data->p_Pos, g_bufferviews[1].byte_lenght);

	// Joints  
	WRITEP(g_exporting_model_data->p_Blendids, g_bufferviews[2].byte_lenght);

	// Weights	
	WRITEP(g_exporting_model_data->p_BlendWeights, g_bufferviews[3].byte_lenght);

	// Normal	
	WRITEP(g_exporting_model_data->p_Normal, g_bufferviews[4].byte_lenght);
	
	// Binormal	
	WRITEP(g_exporting_model_data->p_Binormal, g_bufferviews[5].byte_lenght);

	// Tangent	
	WRITEP(g_exporting_model_data->p_Tangent, g_bufferviews[6].byte_lenght);

	// Texcoord	
	WRITEP(g_exporting_model_data->p_UV, g_bufferviews[7].byte_lenght);

	// Indeces

	for (UINT i = 8; i < amt_accessors; i++)
	{
		WRITEP(g_exporting_model_data->primitives[no_meat_primitives[i - 8]].p_Indecies, g_bufferviews[i].byte_lenght);
	}

	anim_accessor_id = amt_accessors;

	if (g_exporting_model_data->bHaveAnimations)
	{
		gltf_animation* pAnims = g_exporting_model_data->pAnims;

		for (int i = 0; i < g_exporting_model_data->amount_animations; i++)
		{
			for (int j = 0; j < pAnims[i].amount_nodes; j++)
			{
				// keyframes
				WRITEP(pAnims[i].pNodeAnims[j].key_frames, g_bufferviews[anim_accessor_id].byte_lenght);
				anim_accessor_id++;


				// rotations
				WRITEP(pAnims[i].pNodeAnims[j].rotations, g_bufferviews[anim_accessor_id].byte_lenght);
				anim_accessor_id++;

				if (!pAnims[i].pNodeAnims[j].is_not_use_translation)
				{
					WRITEP(pAnims[i].pNodeAnims[j].translations, g_bufferviews[anim_accessor_id].byte_lenght);
					anim_accessor_id++;
				}

			}

			if (pAnims[i].have_origin_anim)
			{
				// keyframes
				WRITEP(pAnims[i].pOriginNodeAnim->key_frames, g_bufferviews[anim_accessor_id].byte_lenght);
				anim_accessor_id++;

				// rotations
				WRITEP(pAnims[i].pOriginNodeAnim->rotations, g_bufferviews[anim_accessor_id].byte_lenght);
				anim_accessor_id++;

				if (!pAnims[i].pOriginNodeAnim->is_not_use_translation)
				{
					WRITEP(pAnims[i].pOriginNodeAnim->translations, g_bufferviews[anim_accessor_id].byte_lenght);
					anim_accessor_id++;
				}
			}
		}
	}

	*pData_len = GetFileSize(f2, NULL);



	CloseHandle(f2);
	return true;
}

bool json_fill_array_floats(json_token* arr_token, float* arr_floats, UINT count)
{
	if (!arr_floats || arr_token->type != TOKEN_ARRAY || arr_token->get_count() < count)
		return false;

	UINT arr_token_id = arr_token->id;

	for (UINT i = 0; i < count; i++)
	{
		g_JP.add_obj_or_arr_member(arr_token_id, TOKEN_FLOAT_NUMBER, nullptr)->set_float(arr_floats[i]);
	}

	return true;
}

bool json_fill_array_uints(json_token* arr_token, UINT* arr_ints, UINT count)
{
	if (!arr_ints || arr_token->type != TOKEN_ARRAY || arr_token->get_count() < count)
		return false;

	UINT arr_token_id = arr_token->id;

	for (UINT i = 0; i < count; i++)
	{
		g_JP.add_obj_or_arr_member(arr_token_id, TOKEN_NUMBER, nullptr)->set_int(arr_ints[i]);
	}

	return true;
}

bool GLTF_fill_nodes()
{
	g_JP.add_obj_or_arr_member(0, TOKEN_ARRAY, (char*)"nodes", g_exporting_model_data->amount_nodes);
	UINT nodes_token = g_JP.get_field(&(g_JP.m_json_tokens.at(0)), "nodes")->id;
	
	if (g_exporting_model_data->amount_nodes == 1)
	{
		UINT first_node = g_JP.add_obj_or_arr_member(nodes_token, TOKEN_OBJ, nullptr, 3)->id;

		g_JP.add_obj_or_arr_member(first_node, TOKEN_STR, (char*)"name")->set_str(g_exporting_model_data->model_name);
		g_JP.add_obj_or_arr_member(first_node, TOKEN_NUMBER, (char*)"mesh")->set_int(0);

		g_JP.add_obj_or_arr_member(first_node, TOKEN_ARRAY, (char*)"rotation", 4);

		float flip_x_axis_quaternion[4] = { 1.0f, 0.0f, 0.0f, 0.0f };

		json_fill_array_floats(g_JP.get_field(&(g_JP.m_json_tokens.at(first_node)), "rotation"), flip_x_axis_quaternion, 4);

		return true;
	}

	UINT first_node = g_JP.add_obj_or_arr_member(nodes_token, TOKEN_OBJ, nullptr, 5)->id;

	g_JP.add_obj_or_arr_member(first_node, TOKEN_STR, (char*)"name")->set_str(g_exporting_model_data->model_name);
	g_JP.add_obj_or_arr_member(first_node, TOKEN_NUMBER, (char*)"mesh")->set_int(0);
	g_JP.add_obj_or_arr_member(first_node, TOKEN_NUMBER, (char*)"skin")->set_int(0);

	g_JP.add_obj_or_arr_member(first_node, TOKEN_ARRAY, (char*)"rotation", 4);

	float flip_x_axis_quaternion[4] = { 1.0f, 0.0f, 0.0f, 0.0f };

	json_fill_array_floats(g_JP.get_field(&(g_JP.m_json_tokens.at(first_node)), "rotation"), flip_x_axis_quaternion, 4);

	json_token* childs_node0 = g_JP.add_obj_or_arr_member(first_node, TOKEN_ARRAY, (char*)"children", 1);
	UINT children_of_node_0 = 1;
	json_fill_array_uints(childs_node0, &children_of_node_0, 1);

	gltf_node_desc* p_nodes = g_exporting_model_data->nodes;

	for (UINT i = 1; i < g_exporting_model_data->amount_nodes; i++)
	{
		bool not_have_childs = p_nodes[i].no_childs;
		
		json_token* node_token = g_JP.add_obj_or_arr_member(nodes_token, TOKEN_OBJ, nullptr, not_have_childs ? 3 : 4);

		UINT node_token_id = node_token->id;

		g_JP.add_obj_or_arr_member(node_token_id, TOKEN_STR, (char*)"name")->set_str(p_nodes[i].name);

		g_JP.add_obj_or_arr_member(node_token_id, TOKEN_ARRAY, (char*)"translation", 3);
		json_fill_array_floats(g_JP.get_field(&(g_JP.m_json_tokens.at(node_token_id)), "translation"), (float*)&(p_nodes[i].translation_v), 3);

		g_JP.add_obj_or_arr_member(node_token_id, TOKEN_ARRAY, (char*)"rotation", 4);
		json_fill_array_floats(g_JP.get_field(&(g_JP.m_json_tokens.at(node_token_id)), "rotation"), (float*)&(p_nodes[i].rotation_v), 4);

		if (not_have_childs)
			continue;

		g_JP.add_obj_or_arr_member(node_token_id, TOKEN_ARRAY, (char*)"children", p_nodes[i].child_amt);
		json_fill_array_uints(g_JP.get_field(&(g_JP.m_json_tokens.at(node_token_id)), "children"), p_nodes[i].childs, p_nodes[i].child_amt);
	}
	
	return true;
}

bool GLTF_fill_skins()
{

	if (g_exporting_model_data->p_inverseBindMatrices == nullptr)
	{
		g_JP.add_obj_or_arr_member(0, TOKEN_ARRAY, (char*)"skins", 0);
		return true;
	}

	json_token* skins_token = g_JP.add_obj_or_arr_member(0, TOKEN_ARRAY, (char*)"skins", 1);


	if (!skins_token) return false;
	
	UINT first_skin = g_JP.add_obj_or_arr_member(skins_token->id, TOKEN_OBJ, nullptr, 3)->id;
	g_JP.add_obj_or_arr_member(first_skin, TOKEN_NUMBER, (char*)"inverseBindMatrices")->set_int(0);
	g_JP.add_obj_or_arr_member(first_skin, TOKEN_NUMBER, (char*)"skeleton")->set_int(1);

	UINT* p_uints = g_joints_hack_arr;

	if (g_exporting_model_data->amount_nodes - 1 > 200)
	{
		p_uints = (UINT*)malloc(sizeof(UINT) * (g_exporting_model_data->amount_nodes - 1));
		for (UINT i = 0; i < g_exporting_model_data->amount_nodes - 1; i++)
			p_uints[i] = i + 1;
	}

	g_JP.add_obj_or_arr_member(first_skin, TOKEN_ARRAY, (char*)"joints", g_exporting_model_data->amount_nodes - 1);
	json_fill_array_uints(g_JP.get_field(&(g_JP.m_json_tokens.at(first_skin)), "joints"), p_uints, g_exporting_model_data->amount_nodes - 1);

	if (p_uints != g_joints_hack_arr)
		free(p_uints);

	return true;
}

bool GLTF_add_channel(UINT channels_token, UINT sampler_id, UINT node, bool only_rotation)
{
	UINT chnl = g_JP.add_obj_or_arr_member(channels_token, TOKEN_OBJ, nullptr, 2)->id;

	g_JP.add_obj_or_arr_member(chnl, TOKEN_NUMBER, (char*)"sampler")->set_int(sampler_id);


	UINT trgt = g_JP.add_obj_or_arr_member(chnl, TOKEN_OBJ, (char*)"target", 2)->id;

	g_JP.add_obj_or_arr_member(trgt, TOKEN_NUMBER, (char*)"node")->set_int(node);
	g_JP.add_obj_or_arr_member(trgt, TOKEN_STR, (char*)"path")->set_str((char*)"rotation");

	if (only_rotation) return true;

	UINT chnl2 = g_JP.add_obj_or_arr_member(channels_token, TOKEN_OBJ, nullptr, 2)->id;

	g_JP.add_obj_or_arr_member(chnl2, TOKEN_NUMBER, (char*)"sampler")->set_int(sampler_id + 1);


	UINT trgt2 = g_JP.add_obj_or_arr_member(chnl2, TOKEN_OBJ, (char*)"target", 2)->id;
	g_JP.add_obj_or_arr_member(trgt2, TOKEN_NUMBER, (char*)"node")->set_int(node);
	g_JP.add_obj_or_arr_member(trgt2, TOKEN_STR, (char*)"path")->set_str((char*)"translation");
	
	return false;
}

bool GLTF_fill_animations()
{
	if (!g_exporting_model_data->bHaveAnimations)
	{
		g_JP.add_obj_or_arr_member(0, TOKEN_ARRAY, (char*)"animations", 0);
		return true;
	}

	UINT animations_amount = g_exporting_model_data->amount_animations;

	UINT anims_accessor = g_animation_start_accessor_id;

	gltf_animation* anims = g_exporting_model_data->pAnims;

	UINT animations_token = g_JP.add_obj_or_arr_member(0, TOKEN_ARRAY, (char*)"animations", animations_amount)->id;

	for (int anim_id = 0; anim_id < animations_amount; anim_id++)
	{
		UINT anim_token = g_JP.add_obj_or_arr_member(animations_token, TOKEN_OBJ, nullptr, 3)->id;

		g_JP.add_obj_or_arr_member(anim_token, TOKEN_STR, (char*)"name")->set_str(anims[anim_id].name);

		UINT amt_channels = 0;
		
		for (int j = 0; j < anims[anim_id].amount_nodes; j++)
		{
			amt_channels += anims[anim_id].pNodeAnims[j].is_not_use_translation ? 1 : 2;
		}

		if (anims[anim_id].have_origin_anim)
		{
			amt_channels += anims[anim_id].pOriginNodeAnim->is_not_use_translation ? 1 : 2;
		}

		UINT channels_token = g_JP.add_obj_or_arr_member(anim_token, TOKEN_ARRAY, (char*)"channels", amt_channels)->id;

		UINT sampler_id = 0;
		

		for (int j = 0; j < anims[anim_id].amount_nodes; j++)
		{
			if (anims[anim_id].pNodeAnims[j].is_not_use_translation)
			{
				GLTF_add_channel(channels_token, sampler_id++, j + 1, true);
			}
			else
			{
				GLTF_add_channel(channels_token, sampler_id, j + 1, false);
				sampler_id += 2;
			}
		}

		if (anims[anim_id].have_origin_anim)
		{
			if (anims[anim_id].pOriginNodeAnim->is_not_use_translation)
			{
				GLTF_add_channel(channels_token, sampler_id++, 0, true);
			}
			else
			{
				GLTF_add_channel(channels_token, sampler_id, 0, false);
				sampler_id += 2;
			}
		}

		UINT sampler_amt = sampler_id;
		UINT samplers_token = g_JP.add_obj_or_arr_member(anim_token, TOKEN_ARRAY, (char*)"samplers", sampler_amt)->id;

		sampler_id = 0;

		for (int j = 0; j < anims[anim_id].amount_nodes; j++)
		{
			if (anims[anim_id].pNodeAnims[j].is_not_use_translation)
			{
				UINT smplr = g_JP.add_obj_or_arr_member(samplers_token, TOKEN_OBJ, nullptr, 2)->id; sampler_id++;

				UINT keys_accessor_id = anims_accessor++;

				g_JP.add_obj_or_arr_member(smplr, TOKEN_NUMBER, (char*)"input")->set_int(keys_accessor_id);
				g_JP.add_obj_or_arr_member(smplr, TOKEN_NUMBER, (char*)"output")->set_int(anims_accessor++);
			}
			else
			{
				UINT smplr = g_JP.add_obj_or_arr_member(samplers_token, TOKEN_OBJ, nullptr, 2)->id; sampler_id++;
				UINT keys_accessor_id = anims_accessor++;

				g_JP.add_obj_or_arr_member(smplr, TOKEN_NUMBER, (char*)"input")->set_int(keys_accessor_id);
				g_JP.add_obj_or_arr_member(smplr, TOKEN_NUMBER, (char*)"output")->set_int(anims_accessor++);

				UINT smplr2 = g_JP.add_obj_or_arr_member(samplers_token, TOKEN_OBJ, nullptr, 2)->id; sampler_id++;
				g_JP.add_obj_or_arr_member(smplr2, TOKEN_NUMBER, (char*)"input")->set_int(keys_accessor_id);
				g_JP.add_obj_or_arr_member(smplr2, TOKEN_NUMBER, (char*)"output")->set_int(anims_accessor++);
			}
		}

		if (anims[anim_id].have_origin_anim)
		{
			if (anims[anim_id].pOriginNodeAnim->is_not_use_translation)
			{
				UINT smplr = g_JP.add_obj_or_arr_member(samplers_token, TOKEN_OBJ, nullptr, 2)->id; sampler_id++;
				UINT keys_accessor_id = anims_accessor++;

				g_JP.add_obj_or_arr_member(smplr, TOKEN_NUMBER, (char*)"input")->set_int(keys_accessor_id);
				g_JP.add_obj_or_arr_member(smplr, TOKEN_NUMBER, (char*)"output")->set_int(anims_accessor++);
			}
			else
			{
				UINT smplr = g_JP.add_obj_or_arr_member(samplers_token, TOKEN_OBJ, nullptr, 2)->id; sampler_id++;
				UINT keys_accessor_id = anims_accessor++;

				g_JP.add_obj_or_arr_member(smplr, TOKEN_NUMBER, (char*)"input")->set_int(keys_accessor_id);
				g_JP.add_obj_or_arr_member(smplr, TOKEN_NUMBER, (char*)"output")->set_int(anims_accessor++);

				UINT smplr2 = g_JP.add_obj_or_arr_member(samplers_token, TOKEN_OBJ, nullptr, 2)->id; sampler_id++;
				g_JP.add_obj_or_arr_member(smplr2, TOKEN_NUMBER, (char*)"input")->set_int(keys_accessor_id);
				g_JP.add_obj_or_arr_member(smplr2, TOKEN_NUMBER, (char*)"output")->set_int(anims_accessor++);
			}
		}

		if (sampler_id != sampler_amt)
		{
			dbgprint(__FUNCTION__, "wrong samplers id calculations suspected \n");
			return false;
		}

	}

	if (anims_accessor != g_accessors_amount)
	{
		dbgprint(__FUNCTION__, "wrong accessor id calculations suspected \n");
		return false;
	}

	return true;
}

bool GLTF_fill_materials()
{
	UINT mat_token = g_JP.add_obj_or_arr_member(0, TOKEN_ARRAY, (char*)"materials", g_exporting_model_data->amount_materials)->id;

	gltf_material_info* mats = g_exporting_model_data->materials;

	for (UINT i = 0; i < g_exporting_model_data->amount_materials; i++)
	{
		UINT count = 2;
		if (mats[i].normal_texture_name) count++;
		if (mats[i].emission_texture_name) count++;
		if (mats[i].specular_texture_name) count++;
		//if (mats[i].alpha_texture_name) count++;

		UINT material_token = g_JP.add_obj_or_arr_member(mat_token, TOKEN_OBJ, nullptr, count)->id;
		
		char* mat_name = (char*)malloc(50);
		ZeroMemory(mat_name, 50);
		sprintf(mat_name, "mat_%08x", mats[i].mat_hash);

		g_JP.add_obj_or_arr_member(material_token, TOKEN_STR, (char*)"name")->set_str(mat_name);

		if (mats[i].normal_texture_name)
		{
			json_token* obj = g_JP.add_obj_or_arr_member(material_token, TOKEN_OBJ, (char*)"normalTexture", 1);
			g_JP.add_obj_or_arr_member(obj->id, TOKEN_NUMBER, (char*)"index")->set_int(mats[i].normal_texture_index);
		}
		
		if (mats[i].emission_texture_name)
		{
			json_token* obj = g_JP.add_obj_or_arr_member(material_token, TOKEN_OBJ, (char*)"emissiveTexture", 1);
			g_JP.add_obj_or_arr_member(obj->id, TOKEN_NUMBER, (char*)"index")->set_int(mats[i].emission_texture_index);
		}

		UINT albed_obj = g_JP.add_obj_or_arr_member(material_token, TOKEN_OBJ, (char*)"pbrMetallicRoughness", 1)->id;

		if (mats[i].albedo_texture_name)
		{
			json_token* obj = g_JP.add_obj_or_arr_member(albed_obj, TOKEN_OBJ, (char*)"baseColorTexture", 1);
			g_JP.add_obj_or_arr_member(obj->id, TOKEN_NUMBER, (char*)"index")->set_int(mats[i].albedo_texture_index);
		}
		else
		{
			json_token* obj = g_JP.add_obj_or_arr_member(albed_obj, TOKEN_ARRAY, (char*)"baseColorFactor", 4);
			float flts[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
			json_fill_array_floats(obj, (float*)&flts, 4);
		}

		if (mats[i].specular_texture_name)
		{
			UINT ext_obj = g_JP.add_obj_or_arr_member(material_token, TOKEN_OBJ, (char*)"extensions", 1)->id;
			UINT obj = g_JP.add_obj_or_arr_member(ext_obj, TOKEN_OBJ, (char*)"KHR_materials_specular", 1)->id;
			g_JP.add_obj_or_arr_member(obj, TOKEN_NUMBER, (char*)"specularTexture")->set_int(mats[i].specular_texture_index);
		}

	}

	return true;
}

UINT calc_texture_amount()
{
	gltf_material_info* mats = g_exporting_model_data->materials;

	UINT texture_amount = 0;

	for (UINT i = 0; i < g_exporting_model_data->amount_materials; i++)
	{
		if (mats[i].normal_texture_name) texture_amount++;
		if (mats[i].albedo_texture_name) texture_amount++;
		if (mats[i].specular_texture_name) texture_amount++;
		if (mats[i].emission_texture_name) texture_amount++;
		//if (mats[i].alpha_texture_name) texture_amount++;
	}

	return texture_amount;
}

bool GLTF_fill_images()
{
	UINT img_token = g_JP.add_obj_or_arr_member(0, TOKEN_ARRAY, (char*)"images", g_texture_amt)->id;

	gltf_material_info* mats = g_exporting_model_data->materials;
	UINT texture_index = 0;

	for (UINT i = 0; i < g_exporting_model_data->amount_materials; i++)
	{
		if (mats[i].normal_texture_name)
		{
			json_token* obj = g_JP.add_obj_or_arr_member(img_token, TOKEN_OBJ, nullptr, 1);
			g_JP.add_obj_or_arr_member(obj->id, TOKEN_STR, (char*)"uri")->set_str(mats[i].normal_texture_name);
			mats[i].normal_texture_index = texture_index;
			texture_index++;
		}

		if (mats[i].albedo_texture_name)
		{
			json_token* obj = g_JP.add_obj_or_arr_member(img_token, TOKEN_OBJ, nullptr, 1);
			g_JP.add_obj_or_arr_member(obj->id, TOKEN_STR, (char*)"uri")->set_str(mats[i].albedo_texture_name);
			mats[i].albedo_texture_index = texture_index;
			texture_index++;
		}

		if (mats[i].specular_texture_name)
		{
			json_token* obj = g_JP.add_obj_or_arr_member(img_token, TOKEN_OBJ, nullptr, 1);
			g_JP.add_obj_or_arr_member(obj->id, TOKEN_STR, (char*)"uri")->set_str(mats[i].specular_texture_name);
			mats[i].specular_texture_index = texture_index;
			texture_index++;
		}

		if (mats[i].emission_texture_name)
		{
			json_token* obj = g_JP.add_obj_or_arr_member(img_token, TOKEN_OBJ, nullptr, 1);
			g_JP.add_obj_or_arr_member(obj->id, TOKEN_STR, (char*)"uri")->set_str(mats[i].emission_texture_name);
			mats[i].emission_texture_index = texture_index;
			texture_index++;
		}
	}

	return true;
}

bool GLTF_fill_meshes()
{
	json_token* meshes_token = g_JP.add_obj_or_arr_member(0, TOKEN_ARRAY, (char*)"meshes", 1);

	if (!meshes_token) return false;

	UINT mesh_token = g_JP.add_obj_or_arr_member(meshes_token->id, TOKEN_OBJ, nullptr, 2)->id;

	g_JP.add_obj_or_arr_member(mesh_token, TOKEN_STR, (char*)"name")->set_str(g_exporting_model_data->model_name);

	UINT prims_token = g_JP.add_obj_or_arr_member(mesh_token, TOKEN_ARRAY, (char*)"primitives", g_exporting_model_data->amount_primitives)->id;

	model_data_primitive* prims = g_exporting_model_data->primitives;

	UINT* valid_ids = g_exporting_model_data->valid_primitives_id;

	UINT indecies_id = 8;


	for (UINT i = 0; i < g_exporting_model_data->amount_primitives; i++)
	{
		UINT prim_token = g_JP.add_obj_or_arr_member(prims_token, TOKEN_OBJ, nullptr, 4)->id;

		bool is_triangle_list_method = g_exporting_model_data->vertex_layout_is_triangle_list;

		g_JP.add_obj_or_arr_member(prim_token, TOKEN_NUMBER, (char*)"mode")->set_int(is_triangle_list_method ? 4 : 5);

		g_JP.add_obj_or_arr_member(prim_token, TOKEN_NUMBER, (char*)"indices")->set_int(indecies_id);
		indecies_id++;

		g_JP.add_obj_or_arr_member(prim_token, TOKEN_NUMBER, (char*)"material")->set_int(prims[valid_ids[i]].mat_id);

		UINT attr = g_JP.add_obj_or_arr_member(prim_token, TOKEN_OBJ, (char*)"attributes", 7)->id;

		g_JP.add_obj_or_arr_member(attr, TOKEN_NUMBER, (char*)"POSITION")->set_int(1);
		g_JP.add_obj_or_arr_member(attr, TOKEN_NUMBER, (char*)"JOINTS_0")->set_int(2);
		g_JP.add_obj_or_arr_member(attr, TOKEN_NUMBER, (char*)"WEIGHTS_0")->set_int(3);
		g_JP.add_obj_or_arr_member(attr, TOKEN_NUMBER, (char*)"NORMAL")->set_int(4);
		g_JP.add_obj_or_arr_member(attr, TOKEN_NUMBER, (char*)"BINORMAL")->set_int(5);
		g_JP.add_obj_or_arr_member(attr, TOKEN_NUMBER, (char*)"TANGENT")->set_int(6);
		g_JP.add_obj_or_arr_member(attr, TOKEN_NUMBER, (char*)"TEXCOORD_0")->set_int(7);

	}

	return true;
}

bool GLTF_fill_buffers(char* path, UINT buff_len)
{
	json_token* buffers_token = g_JP.add_obj_or_arr_member(0, TOKEN_ARRAY, (char*)"buffers", 1);

	if (!buffers_token) return false;

	UINT buffer_token = g_JP.add_obj_or_arr_member(buffers_token->id, TOKEN_OBJ, nullptr, 2)->id;

	g_JP.add_obj_or_arr_member(buffer_token, TOKEN_NUMBER, (char*)"byteLength")->set_int(buff_len);

	char* file_name = strrchr(path, '\\') + 1;

	g_JP.add_obj_or_arr_member(buffer_token, TOKEN_STR, (char*)"uri")->set_str(file_name);

	return true;
}

bool GLTF_fill_bufferViews()
{
	UINT bufferviews_token = g_JP.add_obj_or_arr_member(0, TOKEN_ARRAY, (char*)"bufferViews", g_buffer_views_amount)->id;


	for (UINT i = 0; i < g_buffer_views_amount; i++)
	{
		UINT buffer_view_token = g_JP.add_obj_or_arr_member(bufferviews_token, TOKEN_OBJ, nullptr, 3)->id;
		g_JP.add_obj_or_arr_member(buffer_view_token, TOKEN_NUMBER, (char*)"buffer")->set_int(0);

		g_JP.add_obj_or_arr_member(buffer_view_token, TOKEN_NUMBER, (char*)"byteLength")->set_int(g_bufferviews[i].byte_lenght);
		g_JP.add_obj_or_arr_member(buffer_view_token, TOKEN_NUMBER, (char*)"byteOffset")->set_int(g_bufferviews[i].byte_offset);
	}

	return true; 
}

bool GLTF_fill_accessors()
{
	UINT accessors_token = g_JP.add_obj_or_arr_member(0, TOKEN_ARRAY, (char*)"accessors", g_accessors_amount)->id;

	for (UINT i = 0; i < g_accessors_amount; i++)
	{
		UINT buffer_view_token = g_JP.add_obj_or_arr_member(accessors_token, TOKEN_OBJ, nullptr, 4)->id;
		g_JP.add_obj_or_arr_member(buffer_view_token, TOKEN_NUMBER, (char*)"bufferView")->set_int(g_accessors[i].bufferview_id);

		g_JP.add_obj_or_arr_member(buffer_view_token, TOKEN_NUMBER, (char*)"componentType")->set_int(g_accessors[i].component_type);
		g_JP.add_obj_or_arr_member(buffer_view_token, TOKEN_NUMBER, (char*)"count")->set_int(g_accessors[i].count);

		json_token* acc_type = g_JP.add_obj_or_arr_member(buffer_view_token, TOKEN_STR, (char*)"type");

		switch (g_accessors[i].type)
		{
			case    GL_SCALAR: acc_type->set_str((char*)"SCALAR"); break;
			case	GL_VEC2:   acc_type->set_str((char*)"VEC2"); break;
			case	GL_VEC3:   acc_type->set_str((char*)"VEC3"); break;
			case	GL_VEC4:   acc_type->set_str((char*)"VEC4"); break;
			case	GL_MAT2:   acc_type->set_str((char*)"MAT2"); break;
			case	GL_MAT3:   acc_type->set_str((char*)"MAT3"); break;
			case	GL_MAT4:   acc_type->set_str((char*)"MAT4"); break;
		default:
			dbgprint(__FUNCTION__, "error type %d\n", g_accessors[i].type);
			acc_type->set_str((char*)"UNKNOWN");
			break;
		}

	}


	return true;
}

bool GLTF_fill_textures()
{
	UINT textures_token = g_JP.add_obj_or_arr_member(0, TOKEN_ARRAY, (char*)"textures", g_texture_amt)->id;

	for (int i = 0; i < g_texture_amt; i++)
	{
		UINT obj = g_JP.add_obj_or_arr_member(textures_token, TOKEN_OBJ, nullptr, 2)->id;
		g_JP.add_obj_or_arr_member(obj, TOKEN_NUMBER, (char*)"sampler")->set_int(0);
		g_JP.add_obj_or_arr_member(obj, TOKEN_NUMBER, (char*)"source")->set_int(i);
	}
	return true;
}

bool GLTF_fill_samplers()
{
	UINT smplr_token = g_JP.add_obj_or_arr_member(0, TOKEN_ARRAY, (char*)"samplers", 1)->id;
	g_JP.add_obj_or_arr_member(smplr_token, TOKEN_OBJ, nullptr, 0);
	return true;
}

bool Exporter_data_destructor()
{
	g_JP.flush_data();

	model_data* md = g_exporting_model_data;

	if (md)
	{
		if (md->mmi) _aligned_free(md->mmi);
		md->mmi = nullptr;

		if (md->materials) free(md->materials);
		md->materials = nullptr;

		md->amount_materials = 0;

		if (md->primitives)
		{
			for (int i = 0; i < md->amount_primitives; i++)
			{
				if (md->primitives[i].p_Indecies) free(md->primitives[i].p_Indecies);
			}
			free(md->primitives);
		}
		
		if (md->valid_primitives_id) free(md->valid_primitives_id);
		md->valid_primitives_id = nullptr;

		md->amount_nodes = 0;
		if (md->nodes) free(md->nodes);
		md->nodes = nullptr;

		md->p_inverseBindMatrices = nullptr;

		if( md->p_Pos		   ) free(md->p_Pos);
		if( md->p_Blendids	   ) free(md->p_Blendids);
		if( md->p_BlendWeights ) free(md->p_BlendWeights);
		if( md->p_Tangent	   ) free(md->p_Tangent);
		if( md->p_Binormal	   ) free(md->p_Binormal);
		if( md->p_Normal	   ) free(md->p_Normal);
		if( md->p_UV		   ) free(md->p_UV);
		if( md->p_Unknown	   ) free(md->p_Unknown);

		md->p_Pos		   = nullptr;
		md->p_Blendids	   = nullptr;
		md->p_BlendWeights = nullptr;
		md->p_Tangent	   = nullptr;
		md->p_Binormal	   = nullptr;
		md->p_Normal	   = nullptr;
		md->p_UV		   = nullptr;
		md->p_Unknown	   = nullptr;

		
		if (md->bHaveAnimations)
		{
			if (md->pAnims)
			{
				gltf_animation* anims = md->pAnims;
				for (int anim_id = 0; anim_id < md->amount_animations; anim_id++)
				{
					for (int i = 0; i < anims[anim_id].amount_nodes; i++)
					{
						if (anims[anim_id].pNodeAnims[i].key_frames)   free(anims[anim_id].pNodeAnims[i].key_frames);
						if (anims[anim_id].pNodeAnims[i].rotations)    free(anims[anim_id].pNodeAnims[i].rotations);
						if (anims[anim_id].pNodeAnims[i].translations) free(anims[anim_id].pNodeAnims[i].translations);
					}

					if (anims[anim_id].have_origin_anim)
					{
						if (anims[anim_id].pOriginNodeAnim->key_frames)   free(anims[anim_id].pOriginNodeAnim->key_frames);
						if (anims[anim_id].pOriginNodeAnim->rotations)    free(anims[anim_id].pOriginNodeAnim->rotations);
						if (anims[anim_id].pOriginNodeAnim->translations) free(anims[anim_id].pOriginNodeAnim->translations);
					}
				}
				free(md->pAnims);
			}
		}
		md->bHaveAnimations = false;
		md->pAnims = nullptr;
	}

	if (g_accessors) free(g_accessors);
	g_accessors = nullptr;

	if (g_bufferviews) free(g_bufferviews);
	g_bufferviews = nullptr;

	return true;
}

bool Export(UINT model_id, bool only_model, bool only_current_animation)
{
	if (model_id >= 1000)
	{
		dbgprint(__FUNCTION__, "error model_id excided maximum model amount == 1000 \n");
		return false;
	}

	char mdl_name[MAX_PATH];
	ZeroMemory(mdl_name, MAX_PATH);
	char mdl_addition_file_name[MAX_PATH];
	ZeroMemory(mdl_addition_file_name, MAX_PATH);

	wchar_t* path_where_to_save = nullptr;

	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED |
		COINIT_DISABLE_OLE1DDE);
	if (SUCCEEDED(hr))
	{
		IFileOpenDialog *pFileOpen;

		// Create the FileOpenDialog object.
		hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
			IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

		if (SUCCEEDED(hr))
		{

			FILEOPENDIALOGOPTIONS* pfos = new FILEOPENDIALOGOPTIONS;
			ZeroMemory(pfos, sizeof(pfos));
			pFileOpen->GetOptions(pfos);
			*pfos = *pfos |= FOS_PICKFOLDERS;
			pFileOpen->SetOptions(*pfos);
			pFileOpen->SetTitle(L"Select folder to dump model");
			// Show the Open dialog box.
			hr = pFileOpen->Show(NULL);

			// Get the file name from the dialog box.
			if (SUCCEEDED(hr))
			{
				IShellItem *pItem;
				hr = pFileOpen->GetResult(&pItem);
				if (SUCCEEDED(hr))
				{
					hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &path_where_to_save);
					if (FAILED(hr))
					{
						dbgprint("Dump_map", "failed to choose folder with error hr = %x \n", hr);
						//MessageBoxW(NULL, L"Bad folder or else ...", L"File Path", MB_OK);
						return false;
					}
					pItem->Release();
				}
			}

			pFileOpen->Release();
		}
		CoUninitialize();
	}

	char User_selected_path[MAX_PATH];
	ZeroMemory(User_selected_path, MAX_PATH);
	wcstombs(User_selected_path, path_where_to_save, MAX_PATH);

	strcat(mdl_addition_file_name, User_selected_path);
	strcat(mdl_addition_file_name, "\\");
	strcat(mdl_addition_file_name, g_mmi[model_id].model_name);
	strcat(mdl_addition_file_name, ".bin");

	strcat(mdl_name, User_selected_path);
	strcat(mdl_name, "\\");
	strcat(mdl_name, g_mmi[model_id].model_name);
	strcat(mdl_name, ".gltf");

	wchar_t cur_model_name_W[MAX_PATH] = { 0 };
	mbstowcs(cur_model_name_W, g_mmi[model_id].model_name, MAX_PATH);

	wchar_t file_path[MAX_PATH] = { 0 };

	wsprintf(file_path, L"%ws\\", g_path1);
	lstrcat(file_path, cur_model_name_W);
	lstrcat(file_path, L".RSCF");

	if (!Read_File_and_fill_vertex_data(file_path))
	{
		dbgprint(__FUNCTION__, "Error reading model file\n");
		return false;
	}

	g_exporting_model_data->model_id = model_id;

	if (!Fill_nodes_info())
	{
		dbgprint(__FUNCTION__, "Error Fill_nodes_info\n");
		return false;
	}

	Fill_material_info(User_selected_path);

	UINT data_file_len = 0;

	if (!only_model)
		fill_animations_info(only_current_animation);
	else
		g_exporting_model_data->bHaveAnimations = false;

	if (!Fill_data_file(mdl_addition_file_name, &data_file_len))
	{
		dbgprint(__FUNCTION__, "Error writing data file\n");
		return false;
	}


	// .gltf json file structure
	// {
	//   "asset" : {},
	//   "scene" : 0,
	//   "scenes" : [ { "nodes" : [ 0 ] } ],
	//   "nodes" : [...],
	//   "skins" : [...],
	//   "animations" : [...],
	//   "materials" : [...],
	//   "images" : [...],
	//   "meshes" : [ { "primitives" : [...], "name" : "mdl_name" } ],
	//   "buffers" : [ { "uri" : "data_file", "byteLenght" : +inf } ],
	//   "bufferViews" : [...],
	//   "accessors" : [...],
	//   "textures" : [...],
	//   "samplers" : [ {} ]
	// }


	g_JP.m_root_token = g_JP.create_obj_or_arr(only_model ? 13 : 14, (char*)"ROOT", true);

	UINT asset_token = g_JP.add_obj_or_arr_member(0, TOKEN_OBJ, (char*)"asset", 2)->id;



	g_JP.add_obj_or_arr_member(asset_token, TOKEN_STR, "generator")->set_str((char*)"AVP2010ModelViewer");
	g_JP.add_obj_or_arr_member(asset_token, TOKEN_STR, "version")->set_str((char*)"2.0");

	//   "scene" : 0,
	g_JP.add_obj_or_arr_member(0, TOKEN_NUMBER, (char*)"scene")->set_int(0);

	//   "scenes" : [ { "nodes" : [ 0 ] } ],
	UINT scenes_token1 = g_JP.add_obj_or_arr_member(0, TOKEN_ARRAY, (char*)"scenes", 1)->id;
	
	
	json_token* scenes_token = g_JP.add_obj_or_arr_member(scenes_token1, TOKEN_OBJ, nullptr, 1);
	json_token* scene_token = g_JP.get_array_member(scenes_token, 0);
	g_JP.add_obj_or_arr_member(scene_token->id, TOKEN_ARRAY, "nodes", 1);
	g_JP.add_obj_or_arr_member(g_JP.get_field(scene_token, "nodes")->id, TOKEN_NUMBER, nullptr, 1)->set_int(0);

	int load_status = 0;

	//   "nodes" : [...],
	load_status += GLTF_fill_nodes();
	//dbgprint(__FUNCTION__, "nodes completed\n");
	// "skins" : [...],
	load_status += GLTF_fill_skins();
	//dbgprint(__FUNCTION__, "skins completed\n");
	if(!only_model)
		load_status += GLTF_fill_animations();

	//   "images" : [...],
	g_texture_amt = calc_texture_amount();

	load_status += GLTF_fill_images();
	//dbgprint(__FUNCTION__, "images completed\n");
	//   "materials" : [...],
	load_status += GLTF_fill_materials();
	//dbgprint(__FUNCTION__, "materials completed\n");
	
	//   "meshes" : [ { "primitives" : [...], "name" : "mdl_name" } ],
	load_status += GLTF_fill_meshes();
	//dbgprint(__FUNCTION__, "meshes completed\n");
	//   "buffers" : [ { "uri" : "data_file", "byteLenght" : +inf } ],
	load_status += GLTF_fill_buffers(mdl_addition_file_name, data_file_len);
	//dbgprint(__FUNCTION__, "buffers completed\n");
	//   "bufferViews" : [...],
	load_status += GLTF_fill_bufferViews();
	//dbgprint(__FUNCTION__, "bufferViews completed\n");
	//   "accessors" : [...]
	load_status += GLTF_fill_accessors();
	//dbgprint(__FUNCTION__, "accessors completed\n");
	load_status += GLTF_fill_textures();

	load_status += GLTF_fill_samplers();

	if (load_status != (only_model ? 10 : 11))
	{
		dbgprint(__FUNCTION__, "error filling gltf content\n");
		return false;
	}

	// HACK! HACK! HACK!
	// I HATE VECTORS !
	g_JP.m_root_token = &(g_JP.m_json_tokens.at(0));

	UINT size_gltf_file = g_JP.json_estimate_serialized_str_size();
	char* gltf_content = (char*)malloc(size_gltf_file + 1);
	ZeroMemory(gltf_content, size_gltf_file);
	g_JP.json_serialize(gltf_content);

	HANDLE f2 = CreateFileA(mdl_name, GENERIC_WRITE, NULL, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	DWORD a = 0;
	WRITEP(gltf_content, size_gltf_file);
	free(gltf_content);
	CloseHandle(f2);

	dbgprint(__FUNCTION__, "succesfully dumped model file %s\n", mdl_name);

	Exporter_data_destructor();

	return true;
}
