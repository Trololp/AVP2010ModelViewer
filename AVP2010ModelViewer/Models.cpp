#include "stdafx.h"
#include "Models.h"
#include "Debug.h"
#include "Hash_tree.h"
#include "Asura_headers.h"
#include "Console.h"
#include "umHalf.h"
#include "Model_Exporter_gltf.h"

struct MARE_ENTRY
{
	DWORD mat_hash;
	unsigned long int num;
	DWORD hash_albedo;
	DWORD hash_normal;
	DWORD hash_specular;
	DWORD hash_alpha;
	DWORD hash_emission;
	float a;
	float b[4];
	DWORD c[3];
	DWORD Hash6;
	DWORD d;
	DWORD e[4];
	DWORD f[2];
	DWORD h[3][4];
	DWORD g;
	DWORD i[6];
	DWORD j[3];
	DWORD l[6];
	DWORD Default_mat_id;
	DWORD Hash7;
	DWORD m;
	DWORD Hash8;
	DWORD n;
	DWORD Hash9;
	DWORD Hash10;
	DWORD k[4];
	DWORD pad[16];
};

//Extern Global variables
extern ID3D11Device*           g_pd3dDevice;
extern hash_tree_node2* model_hashs;
extern hash_tree_node2* model_skeletons;
extern hash_tree_node2* model_skeleton_anims;
extern hash_tree_node2* model_HMPTs;
extern wchar_t* g_path1;
extern hash_tree_node* hash_tree_texture;
extern std::vector <bbox>		g_bboxes;
extern DWORD g_selected_model_index;
extern DWORD g_selected_anim_index;
extern XMMATRIX				g_Matrix_Identity;
extern avp_texture AVP_TEXTURES[1500];

// Globals
DWORD g_loaded_mdl_count = 0;
DWORD g_loaded_anim_count = 0;

// Connection betwen this two didnt found yet. So basicaly you must specify
// model and anim by hands. 
model_incremental_info g_mmi[1000] = { 0 }; // global model information
anim_incremental_info g_anm[2000] = { 0 }; // global animation information.

// Animation matrixes that will be passed to shader
XMMATRIX g_axBoneMatrices[128];

DWORD Command_dump_anim(DWORD*);
DWORD Command_dump_model(DWORD*);

Command cmd_dump_anim = {
	"dump_anim",
	CONCMD_RET_ZERO,
	CONARG_END,
	Command_dump_anim,
	nullptr
};

Command cmd_dump_model = {
	"dump_model",
	CONCMD_RET_ZERO,
	CONARG_STR,
	Command_dump_model,
	nullptr
};

int Read_models(const wchar_t* models_folder_path)
{
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind = FindFirstFile(models_folder_path, &FindFileData);
	if (hFind == INVALID_HANDLE_VALUE) {
		dbgprint("Models", "FindFirstFile failed (%d)\n", GetLastError());
		return 0;
	}

	do {
		if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			continue;
		else
			Read_model(FindFileData.cFileName, 0);
	} while (FindNextFile(hFind, &FindFileData) != 0);

	FindClose(hFind);
	return 1;
}



int read_padded_str(HANDLE file, char* dest)
{
	bool check = 1;
	DWORD bytes_readen = 0;
	int len = 4;
	int data;
	DWORD offset = SetFilePointer(file, NULL, NULL, FILE_CURRENT);
	ReadFile(file, &data, 4, &bytes_readen, NULL);
	for (; ((data >> 24) && ((data >> 16) & 0xFF ) && ((data >> 8) & 0xFF) && (data & 0xFF));)
	{
		ReadFile(file, &data, 4, &bytes_readen, NULL);
		len += 4;
	}
	SetFilePointer(file, offset, NULL, FILE_BEGIN);
	if (!ReadFile(file, dest, len, &bytes_readen, NULL))
	{
		dbgprint("Error", "Read padded str error %d\n", GetLastError());
		return 0;
	}
	return 1;
}

void Read_model(const wchar_t* model_name, int skip)
{
	if (g_loaded_mdl_count >= 1500)
		return;

	wchar_t file_path[MAX_PATH] = { 0 };
	if (!skip)
	{
		wsprintf(file_path, L"%ws\\", g_path1);
		lstrcat(file_path, model_name);
	}
	else
	{
		lstrcpy(file_path, model_name);
	}
	DWORD bytes_readen = 0;

	HANDLE file = CreateFileW(file_path, GENERIC_READ, NULL, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (GetLastError() != ERROR_SUCCESS)
	{
		dbgprint("Models", "Opening file Error: %d\n", GetLastError());
		return;
	}
	DWORD* asura_header = (DWORD*)malloc(28);
	ReadFile(file, asura_header, 28, &bytes_readen, NULL);
	
	DWORD hdr_version = *(asura_header + 2);

	if (*asura_header != (DWORD)'FCSR' || *(asura_header + 5) != (DWORD)0xF)
	{
		dbgprint("Models", "Opening file Error: BAD_HEADER (HEADER: %08x Type %02d BytesReaden: %d)\n", *asura_header, *(asura_header + 5), bytes_readen);
		CloseHandle(file);
		return;
	}

	free(asura_header);

	char* name_str = (char*)malloc(260);
	read_padded_str(file, name_str);
	//dbgprint("Model", "RSCF package name: %s \n", name_str);
	model_header mdl_header;
	model_info* mdl_info = new model_info;
	mdl_info->mdl_name = name_str;
	mdl_info->empty_model = 0;
	mdl_info->model_index_in_mii = g_loaded_mdl_count;


	if (ReadFile(file, &mdl_header, sizeof(model_header), &bytes_readen, NULL))
	{
		//dbgprint("Models", "Model header: mmi_count: %d, Vertexes: %d, Indexes: %d \n", mdl_header.model_mesh_info_count, mdl_header.vertex_count, mdl_header.index_count);
		if (mdl_header.model_mesh_info_count == 0)
		{
			dbgprint("Model", "Empty model \n");
			mdl_info->empty_model = 1;
			DWORD hash = hash_from_str(0, name_str);
			hash_tree_node2* h_n = new hash_tree_node2;
			h_n->hash = hash;
			h_n->ptr = mdl_info;
			add_hash2(model_hashs, h_n);
			CloseHandle(file);
			return;
		}

		mdl_info->vertex_layout_triangle_list = mdl_header.vertex_layout;

		model_mat_info* mmi_buff = (model_mat_info*)_aligned_malloc(mdl_header.model_mesh_info_count * sizeof(model_mat_info), 16);
		if (!ReadFile(file, mmi_buff, mdl_header.model_mesh_info_count * sizeof(model_mat_info), &bytes_readen, NULL))
		{
			dbgprint("Models", "Read mmi error: %d \n", GetLastError());
			free(name_str);
			CloseHandle(file);
			return;
		}
		/*
		dbgprint("Models", "%s ------------\n", name_str);
		for (int i = 0; i < mdl_header.model_mesh_info_count; i++)
		{
			model_mat_info* mi = &(mmi_buff[i]);
			
			dbgprint("Models", "hash: %08x unk: %08x pts: %d unk: %08x %08x %08x \n",
				mi->mat_hash, mi->unk, mi->points_count, mi->some_offset, mi->unk2, mi->unk3);
		}
		*/
		mdl_info->mmi = mmi_buff;
		//dbgprint("Models", "mmi[0] dump : %X, %d, index_count: %d, %d, %d, %d\n", mmi_buff[0].mat_hash, mmi_buff[0].unk, mmi_buff[0].points_count, mmi_buff[0].unk2,mmi_buff[0].some_offset, mmi_buff[0].unk3);
		mdl_info->mmi_count = mdl_header.model_mesh_info_count;

		void* vertex_buff = _aligned_malloc(mdl_header.vertex_count * 64, 16);


		if (!ReadFile(file, vertex_buff, mdl_header.vertex_count * 64, &bytes_readen, NULL))
		{
			dbgprint("Models", "Read vertex error: %d \n", GetLastError());
			free(name_str);
			CloseHandle(file);
			return;
		}

		// Create vertex buffer
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_IMMUTABLE;
		bd.ByteWidth = 64 * mdl_header.vertex_count;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;
		bd.MiscFlags = 0;
		bd.StructureByteStride = 0;

		ID3D11Buffer* v_buff;

		D3D11_SUBRESOURCE_DATA InitData = {};
		InitData.pSysMem = vertex_buff;
		InitData.SysMemPitch = 0;
		HRESULT hr;
		hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &v_buff);
		if (FAILED(hr))
		{
			dbgprint("Model", "Create vertex buffer failed !!!\n");
			_aligned_free(vertex_buff);
			free(name_str);
			CloseHandle(file);
			return;
		}
		_aligned_free(vertex_buff);

		mdl_info->pVertexBuff = v_buff;

		void* index_buff = _aligned_malloc(mdl_header.index_count * 2, 16);
		if (!ReadFile(file, index_buff, mdl_header.index_count * 2, &bytes_readen, NULL))
		{
			dbgprint("Models", "Read indexes error: %d \n", GetLastError());
			CloseHandle(file);
			return;
		}

		// Create index buffer
		bd = {};
		bd.Usage = D3D11_USAGE_IMMUTABLE;
		bd.ByteWidth = 2 * mdl_header.index_count;
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bd.CPUAccessFlags = 0;
		bd.MiscFlags = 0;
		bd.StructureByteStride = 0;

		ID3D11Buffer* i_buff;

		InitData = {};
		InitData.pSysMem = index_buff;
		InitData.SysMemPitch = 0;
		hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &i_buff);
		if (FAILED(hr))
		{
			dbgprint("Model", "Create index buffer failed !!!\n");
			_aligned_free(index_buff);
			free(name_str);
			CloseHandle(file);
			return;
		}
		_aligned_free(index_buff);

		mdl_info->pIndexBuff = i_buff;
		
		DWORD hash = hash_from_str(0, name_str);
		//dbgprint("debug", "%s - %d (%x) \n", name_str, hash, hash);
		hash_tree_node2* h_n = new hash_tree_node2;
		h_n->hash = hash;
		h_n->ptr = mdl_info;
		add_hash2(model_hashs, h_n);

		// load info in array
		g_mmi[g_loaded_mdl_count].model_name = name_str;
		g_mmi[g_loaded_mdl_count].mi = mdl_info;
		g_loaded_mdl_count++;
	}

	CloseHandle(file);
	return;
}


void Delete_models_data(hash_tree_node2* h_n)
{
	if (h_n)
	{
		if (h_n->bigger)
			Delete_models_data(h_n->bigger);
		if (h_n->smaller)
			Delete_models_data(h_n->smaller);
		model_info* mdl_info = (model_info*)h_n->ptr;
		if (mdl_info->empty_model)
		{
			if (mdl_info->mdl_name)
				free(mdl_info->mdl_name);
			return;
		}
		if (mdl_info->pIndexBuff)
			mdl_info->pIndexBuff->Release();
		if (mdl_info->pVertexBuff)
			mdl_info->pVertexBuff->Release();
		if (mdl_info->mmi)
			_aligned_free(mdl_info->mmi);
		if (mdl_info->mdl_name)
			free(mdl_info->mdl_name);
	}
	return;
}

void Delete_models()
{
	Delete_models_data(model_hashs);
	delete_nodes2(model_hashs);
	return;
}

void Calculate_skeleton(model_skeleton* skl, DWORD index, XMMATRIX* M_prev)
{
	XMFLOAT4 rel_rot_ = skl->pBones[index].rel_rot;
	XMFLOAT3 rel_pos_ = skl->pBones[index].rel_pos;
	XMVECTOR rel_rot_quat = XMLoadFloat4(&rel_rot_);
	XMVECTOR rel_pos = XMLoadFloat3(&rel_pos_);
	XMMATRIX R_i = XMMatrixRotationQuaternion(rel_rot_quat);
	rel_pos.m128_f32[3] = 1.0f;
	XMVECTOR offset = XMVector3Transform(rel_pos, *M_prev);
	XMMATRIX M = XMMatrixMultiply(R_i, *M_prev);
	M.r[3] = offset;
	R_i.r[3] = rel_pos;

	skl->pBones[index].bone_local_matrix = R_i;
	
	skl->pBones[index].bone_model_matrix = M;
	M.r[3] = g_XMIdentityR3;
	XMMATRIX M2 = XMMatrixTranspose(M);
	M2.r[3] = -(XMVector3Transform(offset, M2));
	M2.r[3].m128_f32[3] = 1.0f;
	skl->pBones[index].inv_bone_matrix = M2;
	M.r[3] = offset;

	if (skl->amount_bones <= 1)
		return;

	int i = 1;
	do
	{
		if (skl->pBones[i].parent_bone_id == index)
		{
			Calculate_skeleton(skl, i, &M);
		}
		i++;
	} while (i < skl->amount_bones);
}

void HSKN_some_model_inside_read(HANDLE f)
{
	DWORD a = 0;
	DWORD thunk;
	DWORD type = 0;
	READ(type);
	if (type >= 3)
	{
		DWORD amount_pts;
		DWORD amount_2;
		DWORD val1 = 0;
		DWORD val2 = 0;
		DWORD val3 = 0;

		READ(amount_pts);
		READ(amount_2);
		READ(val1);
		READ(val2);
		if (type >= 9)
			READ(val3); // some flag or what
		WORD w1;
		WORD w2;
		READ(w1);
		READ(w2);

		if (type >= 9)
			READ(thunk);

		asura_bbox bb;
		READ(bb);

		float f_val;
		READ(f_val); // bounding box radius

		//dbgprint("HSKN_submodel", "type %d amount_pts %d amt2 %d f_val %f \n", type, amount_pts, amount_2, f_val);

		SFPC(amount_pts * sizeof(XMFLOAT3));

		//XMFLOAT3* points = (XMFLOAT3*)malloc(amount_pts * sizeof(XMFLOAT3));
		//READP(points, amount_pts * sizeof(XMFLOAT3));
		 
		// There some model inside like same as original
		
		/*
		for (int i = 0; i < amount_pts; i++)
		{
			XMFLOAT3* p = &(points[i]);
			float sz = 0.005f;

			XMFLOAT3 pos2 = { p->x + sz, p->y + sz, p->z + sz };
			XMFLOAT3 pos1 = { p->x - sz, p->y - sz, p->z - sz };


			g_bboxes.push_back({ pos1, pos2, {1.0f, 1.0f, 0.0f, 1.0f} });
		}
		*/
		SFPC(8 * amount_2); // some indexes

		if (val1)
			SFPC(2 * amount_2); // ?
		if (val2)
			SFPC(2 * amount_2); // sequence number of state per point ?
		if (val3)
			SFPC(4 * amount_2); // some hashes
		if (type >= 8)
		{
			DWORD val;
			SFPC(4);
			READ(val);
			if (val)
				SFPC(4 * amount_2);
		}
	}
}

int Read_HSKN(const wchar_t* file_name)
{
	wchar_t file_path[MAX_PATH] = { 0 };
	wsprintf(file_path, L"%ws\\HSKN_chunk\\", g_path1);
	lstrcat(file_path, file_name);

	DWORD a = 0;
	HANDLE f = CreateFileW(file_path, GENERIC_READ, NULL, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (int i = GetLastError() != ERROR_SUCCESS)
	{
		dbgprint("Skeleton", "Opening file Error: %d\n", i);
		CloseHandle(f);
		return 0;
	}

	asura24b_hdr hdr;
	READ(hdr);

	if (hdr.magic != (DWORD)'NKSH')
	{
		dbgprint("Skeleton", "Opening file Error: INCORRECT FILE HEADER (%ws)\n", file_path);
		CloseHandle(f);
		return 0;
	}

	char model_name[200] = { 0 };
	read_padded_str(f, model_name);

	if (hdr.amount1 && (hdr.type2 & 0x40) == 0)
	{
		dbgprint("Skeleton", "found data with type2 & 0x40 flag set \n");
		SFPC(72 * hdr.amount1); // Idk
	}
	model_skeleton* p_mdl_skeleton = new model_skeleton;
	DWORD model_name_hash = hash_from_str(0, model_name);
	DWORD skeleton_bone_count = hdr.amount2;
	p_mdl_skeleton->model_name = model_name;
	p_mdl_skeleton->model_hash = model_name_hash;
	p_mdl_skeleton->amount_bones = skeleton_bone_count;

	DWORD* nodes_ids = (DWORD*)malloc(skeleton_bone_count * 4);
	READP(nodes_ids, (skeleton_bone_count * 4));

	struct skeleton_node_point
	{
		XMFLOAT3 rel_pos;
		XMFLOAT4 rel_rot;
	};

	skeleton_node_point* skl_points = (skeleton_node_point*)malloc(skeleton_bone_count * sizeof(skeleton_node_point));
	READP(skl_points, skeleton_bone_count * sizeof(skeleton_node_point));

	if (hdr.type1 >= 10)
	{
		SFPC(1);
	}

	char** Node_names = (char**)malloc(skeleton_bone_count * 4);

	if ((hdr.type2 & 1) != 0)
	{
		for (int i = 0; i < skeleton_bone_count; i++)
		{
			char* node_name = (char*)malloc(200);
			read_padded_str(f, node_name);

			if (hdr.type1 >= 10)
			{
				SFPC(1);
			}

			Node_names[i] = node_name;
			DWORD tmp;
			READ(tmp);
			if (tmp)
			{
				HSKN_some_model_inside_read(f);
				//dbgprint("Skeleton", "Found generic model in %s skeleton (bone %d) ! \n", model_name, i);
			}
		}
	}
	
	if (hdr.type1 >= 5 && (hdr.type2 & 2) != 0)
	{
		asura_bbox bb;
		READ(bb);
		dbgprint("Skeleton", "found bbox {%f %f %f %f %f %f} \n", 
			bb.x1, bb.y1, bb.z1, bb.x2, bb.y2, bb.z2);
	}

	if (hdr.type1 >= 6 && (hdr.type2 & 4) != 0)
	{
		SFPC(4); // some dword
		HSKN_some_model_inside_read(f);
	}

	DWORD* second_nodes_ids = (DWORD*)malloc(skeleton_bone_count * 4);
	ZeroMemory(second_nodes_ids, skeleton_bone_count * 4);

	if (hdr.type1 >= 7)
	{
		READP(second_nodes_ids, (skeleton_bone_count * 4)); // some indexes
	}

	if ((hdr.type2 & 0x10) != 0 && hdr.type1 >= 8)
	{
		dbgprint("Skeleton", "found data with type2 & 0x10 flag set \n");
		SFPC(28 * hdr.amount2);
	}
	
	if ((hdr.type2 & 0x20) != 0 && hdr.type1 >= 9)
	{
		dbgprint("Skeleton", "found data with type2 & 0x20 flag set \n");
		SFPC(4 * hdr.amount2);
	}

	skeleton_bone* pNodes = (skeleton_bone*)malloc(skeleton_bone_count * sizeof(skeleton_bone));

	for (int i = 0; i < skeleton_bone_count; i++)
	{
		pNodes[i].name = Node_names[i];
		pNodes[i].parent_bone_id = nodes_ids[i];
		pNodes[i].unk_bone = second_nodes_ids[i];
		pNodes[i].rel_pos = skl_points[i].rel_pos;
		pNodes[i].rel_rot = skl_points[i].rel_rot;
	}

	p_mdl_skeleton->pBones = pNodes;

	XMMATRIX ident_matrix = XMMatrixIdentity();
	Calculate_skeleton(p_mdl_skeleton, 0, &ident_matrix);

	free(Node_names);
	free(nodes_ids);
	free(skl_points);
	free(second_nodes_ids);

	hash_tree_node2* h_n = new hash_tree_node2;
	h_n->hash = model_name_hash;
	h_n->ptr = p_mdl_skeleton;
	add_hash2(model_skeletons, h_n);

	CloseHandle(f);
	return 1;
}

void Update_Incremental_MMI_HSKN()
{
	for (int i = 0; i < g_loaded_mdl_count; i++)
	{
		DWORD hash = hash_from_str(0,g_mmi[i].model_name);

		hash_tree_node2* hn_p = search_by_hash2(model_skeletons, hash);

		if (hn_p)
		{
			g_mmi[i].skl = (model_skeleton*)hn_p->ptr;
		}
	}
	return;
}

int Read_HSKNs(const wchar_t* folder_path)
{
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind = FindFirstFile(folder_path, &FindFileData);
	if (hFind == INVALID_HANDLE_VALUE) {
		dbgprint("Skeleton", "FindFirstFile failed (%d) %ws\n", GetLastError(), folder_path);
		return 0;
	}
	do {
		if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			continue;
		}
		else
		{
			Read_HSKN(FindFileData.cFileName);
		}
	} while (FindNextFile(hFind, &FindFileData) != 0);

	FindClose(hFind);
	return 1;
}

void Dump_anim(model_skeleton_anim* anim, model_skeleton* skl)
{
	dbgprint("dump_anim", "%s \n", anim->anim_name);


	int amount_bones1 = anim->amount_bones;

	if (anim->have_extra_bone_anim)
		amount_bones1++;

	for (int i = 0; i < amount_bones1; i++)
	{
		if (skl && (i < (amount_bones1 - 1)))
			dbgprint("dump_anim", "Bone %s \n", skl->pBones[i].name);
		else
			dbgprint("dump_anim", "Bone %d \n", i);

		for (int j = 0; j < anim->bone_anims[i].frames; j++)
		{
			XMFLOAT4 quat = anim->bone_anims[i].rots[j].rot;
			float t = anim->bone_anims[i].rots[j].t_perc;

			if (anim->bone_anims[i].not_change_lenght)
			{
				dbgprint("dump_anim", "Frm %d {%f %f %f %f} t %f \n", j, quat.x, quat.y, quat.z, quat.w, t);
			}
			else
			{
				XMFLOAT3 len = anim->bone_anims[i].lens[j];
				dbgprint("dump_anim", "Frm %d {%f %f %f %f} {%f %f %f} t %f \n", j, quat.x, quat.y, quat.z, quat.w, len.x, len.y, len.z, t);
			}
		}
	}
}

DWORD Command_dump_anim(DWORD* args)
{
	Dump_anim(g_anm[g_selected_anim_index].anim, g_mmi[g_selected_model_index].skl);
	return 0;
}

int Read_HANM(const wchar_t* file_name)
{
	if (g_loaded_anim_count >= 2000)
		return 0;


	wchar_t file_path[MAX_PATH] = { 0 };
	wsprintf(file_path, L"%ws\\HANM_chunk\\", g_path1);
	lstrcat(file_path, file_name);

	DWORD a = 0;
	HANDLE f = CreateFileW(file_path, GENERIC_READ, NULL, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (int i = GetLastError() != ERROR_SUCCESS)
	{
		dbgprint("Anim", "Opening file Error: %d\n", i);
		CloseHandle(f);
		return 0;
	}

	asura16b_hdr hdr;
	READ(hdr);

	if (hdr.magic != (DWORD)'MNAH' || ((hdr.type1 != 16) && (hdr.type1 != 17)))
	{
		dbgprint("Anim", "Opening file Error: INCORRECT FILE HEADER (%ws)\n", file_path);
		CloseHandle(f);
		return 0;
	}

	DWORD amount_bones;
	float total_time;
	READ(amount_bones);
	READ(total_time);

	
	char* anim_name = (char*)malloc(256);
	read_padded_str(f, anim_name);
	
	//dbgprint("Anim", "%s num_bones %d total_time %f \n", anim_name, amount_bones, total_time);


	model_skeleton_anim* skl_anim = new model_skeleton_anim;
	skl_anim->type = hdr.type2;
	skl_anim->anim_name = anim_name;
	skl_anim->anim_hash = hash_from_str(0, anim_name);
	skl_anim->total_time = total_time;
	skl_anim->amount_bones = amount_bones;
	skl_anim->have_extra_bone_anim = (hdr.type2 & 0x10) != 0;

	DWORD amount_bones_1 = skl_anim->have_extra_bone_anim ? amount_bones + 1 : amount_bones;

	model_bone_anim* bone_anims = (model_bone_anim*)malloc(amount_bones_1 * sizeof(model_bone_anim));

	for (int i = 0; i < amount_bones_1; i++) // Read the basics of animations !
	{
		READ(bone_anims[i].frames);
		DWORD frames = bone_anims[i].frames;
		READ(bone_anims[i].not_change_lenght);
		bone_anims[i].rots = (model_bone_rot*)malloc(frames * sizeof(model_bone_rot));
		READP(bone_anims[i].rots, frames * sizeof(model_bone_rot));
		for (int j = 0; j < frames; j++)
		{
			XMFLOAT4 q = bone_anims[i].rots[j].rot;
			float len = sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);

			if (len != 0.0f)
			{
				q.x = q.x / len;
				q.y = q.y / len;
				q.z = q.z / len;
				q.w = q.w / len;
			}

			bone_anims[i].rots[j].rot = q;
		}
		DWORD amount_bone_delta_len = (bone_anims[i].not_change_lenght) ? 1 : frames;
		bone_anims[i].lens = (XMFLOAT3*)malloc(amount_bone_delta_len * sizeof(XMFLOAT3));
		READP(bone_anims[i].lens, amount_bone_delta_len * sizeof(XMFLOAT3));
	}

	skl_anim->bone_anims = bone_anims;

	float unk1;
	DWORD unk2;
	READ(unk1);
	READ(unk2);
	//dbgprint("Anim", "unk1 = %f unk2 = %08x \n", unk1, unk2);

	if ((hdr.type2 & 1) != 0) // ??? some floats like weights ?
	{
		SFPC(amount_bones * 4);
		//DWORD* some_array = (DWORD*)malloc(amount_bones * 4);
		//READP(some_array, amount_bones * 4);
		//Dump_floats("Anim_some_floats", some_array, amount_bones * 4);
		//free(some_array);
	}

	DWORD amount_1;
	READ(amount_1);
	//DWORD* some_array2 = (DWORD*)malloc(amount_1 * 12);
	SFPC(amount_1 * 12);
	//READP(some_array2, amount_1 * 12);
	//dbgprint("Anim", "some_array2 amount elems %d \n", amount_1);
	//Dump_hex("Anim_some_array2_12b", some_array2, amount_1 * 12);
	//free(some_array2);

	if ((hdr.type2 & 8) != 0)
	{
		asura_bbox bbox;
		READ(bbox);
		//Dump_floats("Anim_bbox", &bbox, 24);
	}

	float unk_f;
	READ(unk_f);
	//dbgprint("Anim", "unk_f = %f \n", unk_f);

	DWORD amount_2;
	READ(amount_2);
	SFPC(amount_2 * 16);
	//DWORD* some_array3 = (DWORD*)malloc(amount_2 * 16);
	//READP(some_array3, amount_2 * 16);
	//dbgprint("Anim", "some_array3 amount elems %d \n", amount_2);
	//Dump_hex("Anim_some_array3_16b", some_array3, amount_2 * 16);
	//free(some_array3);

	if ((hdr.type2 & 0x40) != 0)
	{
		SFPC(4 * amount_bones);
		char tmp_str[256] = { 0 };
		for (int i = 0; i < amount_bones; i++)
		{
			read_padded_str(f, tmp_str);
			dbgprint("Anim", "name%d = %s \n", i, tmp_str);
		}
	}

	if ((hdr.type2 & 0x80) != 0)
	{
		SFPC(16 * amount_bones);
	}

	if ((hdr.type2 & 0x400) != 0)
	{
		SFPC(28 * amount_bones);
	}

	if ((hdr.type2 & 0x800) != 0)
	{
		SFPC(28 * amount_bones);
	}

	if (hdr.type1 >= 17)
	{
		BYTE unk_b;
		DWORD unk_dw1;
		DWORD unk_dw2;
		READ(unk_b);
		READ(unk_dw1);
		READ(unk_dw2);
	}

	hash_tree_node2* h_n = new hash_tree_node2;
	h_n->hash = skl_anim->anim_hash;
	h_n->ptr = skl_anim;
	add_hash2(model_skeleton_anims, h_n);

	g_anm[g_loaded_anim_count].anim = skl_anim;
	g_anm[g_loaded_anim_count].anim_name = anim_name;
	g_anm[g_loaded_anim_count].bones_count = amount_bones;
	g_anm[g_loaded_anim_count].current_anim_time = 0.0f;
	g_anm[g_loaded_anim_count].total_time = total_time;

	int max_frames = 0;

	for (int i = 0; i < skl_anim->amount_bones; i++)
	{
		if (max_frames < skl_anim->bone_anims[i].frames)
			max_frames = skl_anim->bone_anims[i].frames;
	}

	g_anm[g_loaded_anim_count].total_frames = max_frames;
	g_anm[g_loaded_anim_count].current_frame = 0;

	//Dump_anim(skl_anim);

	g_loaded_anim_count++;

	CloseHandle(f);
	return 1;
}


int Anim_search_for_nearest_t(model_bone_anim* bone_anim, float t, float T)
{
	if (bone_anim->rots[bone_anim->frames - 1].t_perc*T < t) // t is greater than T somehow.
		return bone_anim->frames - 1;

	if (bone_anim->frames == 1)
	{
		return 0;
	}

	unsigned int start_index = 0;
	unsigned int end_index = bone_anim->frames - 1;

	model_bone_rot* frms = bone_anim->rots;

	while (start_index <= end_index) {
		int j = start_index + (end_index - start_index) / 2;
		if ((frms[j].t_perc*T <= t) && (t <= frms[j + 1].t_perc*T))
			return j;
		if (frms[j].t_perc*T < t)
			start_index = j + 1;
		else
			end_index = j - 1;
	}
	
	return 0;
}
void Skeleton_setup_bone_anim_matrices()
{
	//dbgprint("Anim_debug", "Skeleton_setup_bone_matrices called ! \n");

	for (int i = 0; i < 128; i++)
	{
		g_axBoneMatrices[i] = XMMatrixIdentity();
	}
}

// This is global movement. Model will change position in space.
int Model_anim_update_init(model_skeleton* skl, model_skeleton_anim* anim, float t)
{
	if (!skl || !anim || (skl->amount_bones != anim->amount_bones))
		return 0;

	float T = anim->total_time;
	float k_factor = 0.0f;

	if (anim->have_extra_bone_anim)
	{
		// Get last bone. 
		model_bone_anim* last_bone = &(anim->bone_anims[anim->amount_bones]);

		XMMATRIX M_model_transfor = XMMatrixIdentity();

		if (last_bone->frames > 0)
		{
			int j = Anim_search_for_nearest_t(last_bone, t, anim->total_time);

			XMFLOAT4 quat_rot = last_bone->rots[j].rot;

			XMMATRIX Anim_R = XMMatrixRotationQuaternion(XMLoadFloat4(&quat_rot));
			XMMATRIX Anim_T = XMMatrixIdentity();

			if (!(last_bone->not_change_lenght))
			{
				XMFLOAT3 delta_len = last_bone->lens[j];
				XMVECTOR new_len = XMLoadFloat3(&delta_len);
				Anim_T = XMMatrixTranslationFromVector(new_len);
			}

			if ((j < (last_bone->frames - 1)) && last_bone->frames > 1)
			{
				k_factor = (t - last_bone->rots[j].t_perc*T) / 
					(last_bone->rots[j + 1].t_perc*T - last_bone->rots[j].t_perc*T);
				XMFLOAT4 quat_rot2 = last_bone->rots[j + 1].rot;

				XMVECTOR quat_vec1 = XMLoadFloat4(&quat_rot);
				XMVECTOR quat_vec2 = XMLoadFloat4(&quat_rot2);

				quat_vec1 = XMQuaternionSlerp(quat_vec1, quat_vec2, k_factor);
				Anim_R = XMMatrixRotationQuaternion(quat_vec1);

				if (!(last_bone->not_change_lenght))
				{

					XMFLOAT3 delta_len = last_bone->lens[j];
					XMFLOAT3 delta_len2 = last_bone->lens[j + 1];
					XMVECTOR new_len = XMLoadFloat3(&delta_len);
					XMVECTOR new_len2 = XMLoadFloat3(&delta_len2);
					new_len = XMVectorLerp(new_len, new_len2, k_factor);

					Anim_T = XMMatrixTranslationFromVector(new_len);
				}
			}

			M_model_transfor = XMMatrixMultiply(Anim_R, Anim_T);

			Model_anim_update(skl, anim, t, 0, &M_model_transfor);
			return 1;
		}
	}
	Model_anim_update(skl, anim, t, 0, &g_Matrix_Identity);
	return 1;
}

int Model_anim_update(model_skeleton* skl, model_skeleton_anim* anim, float t, DWORD index, XMMATRIX* M_prev)
{
	if (!skl || !anim || (skl->amount_bones != anim->amount_bones))
		return 0;

	model_bone_anim* bone_anim = &(anim->bone_anims[index]);

	XMMATRIX M_model_transfor;
	float T = anim->total_time;
	float k_factor = 0.0f;

	if (bone_anim->frames > 0)
	{
		int j = Anim_search_for_nearest_t(bone_anim, t, anim->total_time);

		XMFLOAT4 quat_rot = bone_anim->rots[j].rot;

		XMMATRIX Anim_R = XMMatrixRotationQuaternion(XMLoadFloat4(&quat_rot));
		XMMATRIX Anim_T = XMMatrixIdentity();

		if (!(anim->bone_anims[index].not_change_lenght))
		{
			XMFLOAT3 delta_len = bone_anim->lens[j];
			XMVECTOR new_len = XMLoadFloat3(&delta_len);
			new_len = XMVector3Transform(new_len, *M_prev);
			Anim_T = XMMatrixTranslationFromVector(new_len);
		}
		else
		{
			XMMATRIX bone_local_matrix = skl->pBones[index].bone_local_matrix;
			XMVECTOR new_len = bone_local_matrix.r[3];
			new_len = XMVector3Transform(new_len, *M_prev);
			Anim_T = XMMatrixTranslationFromVector(new_len);
		}
		// is have atleast 2 frames to interpolate and not last frame
		if ((j < (bone_anim->frames - 1)) && bone_anim->frames > 1) 
		{
			k_factor = (t - bone_anim->rots[j].t_perc*T) /
				(bone_anim->rots[j + 1].t_perc*T - bone_anim->rots[j].t_perc*T);
			XMFLOAT4 quat_rot2 = bone_anim->rots[j + 1].rot;

			XMVECTOR quat_vec1 = XMLoadFloat4(&quat_rot);
			XMVECTOR quat_vec2 = XMLoadFloat4(&quat_rot2);

			quat_vec1 = XMQuaternionSlerp(quat_vec1, quat_vec2, k_factor);
			Anim_R = XMMatrixRotationQuaternion(quat_vec1);

			if (!(anim->bone_anims[index].not_change_lenght))
			{
				XMFLOAT3 delta_len = bone_anim->lens[j];
				XMFLOAT3 delta_len2 = bone_anim->lens[j + 1];
				XMVECTOR new_len = XMLoadFloat3(&delta_len);
				XMVECTOR new_len2 = XMLoadFloat3(&delta_len2);
				new_len = XMVectorLerp(new_len, new_len2, k_factor);
				new_len = XMVector3Transform(new_len, *M_prev);
				Anim_T = XMMatrixTranslationFromVector(new_len);
			}
		}

		Anim_R = XMMatrixMultiply(Anim_R, *M_prev);
		Anim_R.r[3] = g_XMIdentityR3;

		M_model_transfor = XMMatrixMultiply(Anim_R, Anim_T);

		XMMATRIX Inv_m = skl->pBones[index].inv_bone_matrix;

		g_axBoneMatrices[index] = XMMatrixMultiply(Inv_m, M_model_transfor);
	}
	else
	{
		XMMATRIX bone_local_matrix = skl->pBones[index].bone_local_matrix;
		M_model_transfor = XMMatrixMultiply(bone_local_matrix, *M_prev);
		XMMATRIX Inv_m = skl->pBones[index].inv_bone_matrix;
		g_axBoneMatrices[index] = XMMatrixMultiply(Inv_m, M_model_transfor);
	}

	if (anim->amount_bones <= 1)
		return 0;

	int i = 1;
	do
	{
		if (skl->pBones[i].parent_bone_id == index)
		{
			Model_anim_update(skl, anim, t, i, &M_model_transfor);
		}
		i++;
	} while (i < anim->amount_bones);

	return 1;
}



int Read_HANMs(const wchar_t* folder_path)
{
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind = FindFirstFile(folder_path, &FindFileData);
	if (hFind == INVALID_HANDLE_VALUE) {
		dbgprint("Skeleton", "FindFirstFile failed (%d) %ws\n", GetLastError(), folder_path);
		return 0;
	}
	do {
		if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			continue;
		}
		else
		{
			Read_HANM(FindFileData.cFileName);
		}
	} while (FindNextFile(hFind, &FindFileData) != 0);

	FindClose(hFind);
	return 1;
}

int Read_HSBB(const wchar_t* file_name)
{
	wchar_t file_path[MAX_PATH] = { 0 };
	wsprintf(file_path, L"%ws\\HSBB_chunk\\", g_path1);
	lstrcat(file_path, file_name);

	DWORD a = 0;
	HANDLE f = CreateFileW(file_path, GENERIC_READ, NULL, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (int i = GetLastError() != ERROR_SUCCESS)
	{
		dbgprint("HSBB", "Opening file Error: %d\n", i);
		CloseHandle(f);
		return 0;
	}

	asura16b_hdr hdr;
	READ(hdr);

	if (hdr.magic != (DWORD)'BBSH' || (hdr.type1 != 1))
	{
		dbgprint("HSBB", "Opening file Error: INCORRECT FILE HEADER (%ws)\n", file_path);
		CloseHandle(f);
		return 0;
	}

	char model_name[256] = { 0 };
	read_padded_str(f, model_name);
	DWORD num_bones;
	READ(num_bones);
	DWORD model_name_hash = hash_from_str(0, model_name);
	
	hash_tree_node2* hn_p = search_by_hash2(model_skeletons, model_name_hash);
	model_skeleton* skl = nullptr;


	if (hn_p)
	{
		skl = (model_skeleton*)(hn_p->ptr);
	}
	else
	{
		dbgprint("HSBB", "HSKN for %s not found \n", model_name);
		CloseHandle(f);
		return 0;
	}
	
	if (num_bones != skl->amount_bones)
	{
		dbgprint("HSBB", "num_bones != skl->amount_bones \n");
		CloseHandle(f);
		return 0;
	}

	struct HSBB_entry
	{
		float x1;
		float x2;
		float y1;
		float y2;
		float z1;
		float z2;
		bool active;
	};

	HSBB_entry* HSBB_entries = (HSBB_entry*)malloc(num_bones * sizeof(HSBB_entry));
	READP(HSBB_entries, num_bones * sizeof(HSBB_entry));

	std::vector<bbox> HSBB_bboxes;

	for (int i = 0; i < skl->amount_bones; i++)
	{
		HSBB_entry* e = &(HSBB_entries[i]);

		if (!e->active)
		{
			//dbgprint("HSBB", "Non active bbox %d \n", i);
			continue;
		}

		XMMATRIX Rot_M = skl->pBones[i].bone_model_matrix;
		XMVECTOR p_pos = Rot_M.r[3];
		XMVECTOR quat = XMQuaternionRotationMatrix(Rot_M);
		XMFLOAT3 pos1 = { e->x1, e->y1, e->z1 };
		XMFLOAT3 pos2 = { e->x2, e->y2, e->z2 };

		bbox bounds = { pos1, pos2,{ 0.0f, 1.0f, 1.0f, 0.3f }};
		bbox* normal_bb = shifted_bbox_to_normal(p_pos, &bounds, quat);

		HSBB_bboxes.push_back(*normal_bb);
		delete normal_bb;
	}

	hash_tree_node2* hn_p2 = search_by_hash2(model_hashs, model_name_hash);
	model_info* mdl_info = nullptr;


	if (hn_p2)
	{
		ID3D11Buffer* points_buffer = nullptr;
		DWORD points_HSBB = 0;
		bbox_to_vertex_to_buffer(HSBB_bboxes, &points_HSBB, &points_buffer);

		mdl_info = (model_info*)(hn_p2->ptr);
		DWORD index = mdl_info->model_index_in_mii;

		g_mmi[index].static_HSBB_buffer = points_buffer;
		g_mmi[index].points_HSBB = points_HSBB;
	}
	else
	{
		dbgprint("HSBB", "Model %s not found \n", model_name);
		CloseHandle(f);
		return 0;
	}
	

	free(HSBB_entries);

	CloseHandle(f);
	return 1;
}

int Read_HSBBs(const wchar_t* folder_path)
{
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind = FindFirstFile(folder_path, &FindFileData);
	if (hFind == INVALID_HANDLE_VALUE) {
		dbgprint("HSBB", "FindFirstFile failed (%d) %ws\n", GetLastError(), folder_path);
		return 0;
	}
	do {
		if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			continue;
		}
		else
		{
			Read_HSBB(FindFileData.cFileName);
		}
	} while (FindNextFile(hFind, &FindFileData) != 0);

	FindClose(hFind);
	return 1;
}

int Read_HMPT(const wchar_t* file_name)
{
	wchar_t file_path[MAX_PATH] = { 0 };
	wsprintf(file_path, L"%ws\\HMPT_chunk\\", g_path1);
	lstrcat(file_path, file_name);

	DWORD a = 0;
	HANDLE f = CreateFileW(file_path, GENERIC_READ, NULL, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (int i = GetLastError() != ERROR_SUCCESS)
	{
		dbgprint("HMPT", "Opening file Error: %d\n", i);
		CloseHandle(f);
		return 0;
	}

	asura16b_hdr hdr;
	READ(hdr);

	if (hdr.magic != (DWORD)'TPMH' || (hdr.type1 != 1))
	{
		dbgprint("HMPT", "Opening file Error: INCORRECT FILE HEADER (%ws)\n", file_path);
		CloseHandle(f);
		return 0;
	}


	
	char model_name[256] = { 0 };
	DWORD num_entrys;
	READ(num_entrys);
	read_padded_str(f, model_name);
	dbgprint("HMPT", "read name %s count %d\n", model_name, num_entrys);
	DWORD model_name_hash = hash_from_str(0, model_name);

	

	HMPT_entry* HMPT_entrys = (HMPT_entry*)malloc(num_entrys * sizeof(HMPT_entry));

	for (int i = 0; i < num_entrys; i++)
	{
		READ(HMPT_entrys[i].pos);
		READ(HMPT_entrys[i].unk2);
		READ(HMPT_entrys[i].q1);
		READ(HMPT_entrys[i].q2);
		READ(HMPT_entrys[i].unk); // bone ?

		char* name_str = (char*)malloc(512);
		ZeroMemory(name_str, 512);
		read_padded_str(f, name_str);

		HMPT_entrys[i].name = name_str;

		dbgprint("HMPT", "{%f %f %f}, %f, %d %s \n", HMPT_entrys[i].pos.x, HMPT_entrys[i].pos.y, HMPT_entrys[i].pos.z, 
			HMPT_entrys[i].unk2, HMPT_entrys[i].unk, HMPT_entrys[i].name);
	}


	HMPT_info* hmpti = new HMPT_info;

	hmpti->count = num_entrys;
	hmpti->e = HMPT_entrys;

	hash_tree_node2* h_n = new hash_tree_node2;
	h_n->hash = model_name_hash;
	h_n->ptr = hmpti;
	add_hash2(model_HMPTs, h_n);

	//free(HMPT_entrys);

	CloseHandle(f);
	return 1;
}

void Update_Incremental_MMI_HMPT()
{
	for (int i = 0; i < g_loaded_mdl_count; i++)
	{
		DWORD hash = hash_from_str(0, g_mmi[i].model_name);

		hash_tree_node2* hn_p = search_by_hash2(model_HMPTs, hash);

		if (hn_p)
		{
			dbgprint("HMPT", "set info to %s model\n", g_mmi[i].model_name);
			g_mmi[i].HMPT_info = (HMPT_info*)hn_p->ptr;
		}
	}
	return;
}

int Read_HMPTs(const wchar_t* folder_path)
{
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind = FindFirstFile(folder_path, &FindFileData);
	if (hFind == INVALID_HANDLE_VALUE) {
		dbgprint("HMPT", "FindFirstFile failed (%d) %ws\n", GetLastError(), folder_path);
		return 0;
	}
	do {
		if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			continue;
		}
		else
		{
			Read_HMPT(FindFileData.cFileName);
		}
	} while (FindNextFile(hFind, &FindFileData) != 0);

	FindClose(hFind);
	return 1;
}

int Read_MARE(const wchar_t* file_name)
{
	wchar_t file_path[MAX_PATH] = { 0 };
	wsprintf(file_path, L"%ws\\MARE_chunk\\", g_path1);
	lstrcat(file_path, file_name);

	DWORD bytes_readen = 0;
	HANDLE file = CreateFileW(file_path, GENERIC_READ, NULL, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (int i = GetLastError() != ERROR_SUCCESS)
	{
		dbgprint("Materials", "Opening file Error: %d\n", i);
		CloseHandle(file);
		return 0;
	}

	asura16b_hdr file_header;
	ReadFile(file, &file_header, 16, &bytes_readen, NULL);

	if (file_header.magic != (DWORD)'ERAM' || (file_header.type1 != 21))
	{
		dbgprint("Materials", "Opening file Error: INCORRECT FILE HEADER (%ws)\n", file_path);
		CloseHandle(file);
		return 0;
	}

	unsigned int mat_entries;
	ReadFile(file, &mat_entries, 4, &bytes_readen, NULL);
	int i = 0;
	MARE_ENTRY* entries_mem = (MARE_ENTRY*)malloc(312 * mat_entries);
	ReadFile(file, entries_mem, 312 * mat_entries, &bytes_readen, NULL);

	dbgprint("Materials", "MARE Header: %d materials in file\n", mat_entries);

	hash_tree_node* h_n;

	while (i <= mat_entries)
	{
		if (search_mat_by_hash(entries_mem[i].mat_hash))
		{
			i++;
			continue;
		}
		Material* curr_mat = new Material;
		curr_mat->mat_hash = entries_mem[i].mat_hash;
		curr_mat->mat_num = entries_mem[i].num;

		h_n = search_by_hash(hash_tree_texture, entries_mem[i].hash_albedo);
		//dbgprint("Materials", "%x = search_by_hash( %x , %x ) \n", h_n, hash_tree_texture, entries_mem[i].Hash1);
		if (h_n)
		{
			//dbgprint("Materials", "Found match with texture id %d and material hash %x \n", h_n->texture->id, curr_mat->mat_hash);
			curr_mat->texture_diff_id = h_n->texture->id;
		}
		else
		{
			dbgprint("Materials", "Not found match with texture hash %x and material hash %x \n", entries_mem[i].hash_albedo, curr_mat->mat_hash);
			curr_mat->texture_diff_id = 0;
		}

		h_n = search_by_hash(hash_tree_texture, entries_mem[i].hash_normal);
		//dbgprint("Materials", "%x = search_by_hash( %x , %x ) \n", h_n, hash_tree_texture, entries_mem[i].Hash1);
		if (h_n)
		{
			//dbgprint("Materials", "Found match with texture id %d and material hash %x \n", h_n->texture->id, curr_mat->mat_hash);
			curr_mat->texture_norm_id = h_n->texture->id;
		}
		else
		{
			dbgprint("Materials", "Not found match with texture hash %x and material hash %x \n", entries_mem[i].hash_normal, curr_mat->mat_hash);
			curr_mat->texture_norm_id = 0;
		}

		// specular
		h_n = search_by_hash(hash_tree_texture, entries_mem[i].hash_specular);
		if (h_n)
			curr_mat->texture_spec_id = h_n->texture->id;
		else
			dbgprint("Materials", "Not found match with texture hash %x and material hash %x \n", entries_mem[i].hash_specular, curr_mat->mat_hash);

		// alpha
		h_n = search_by_hash(hash_tree_texture, entries_mem[i].hash_alpha);
		if (h_n)
			curr_mat->texture_alpha_id = h_n->texture->id;
		else
			dbgprint("Materials", "Not found match with texture hash %x and material hash %x \n", entries_mem[i].hash_alpha, curr_mat->mat_hash);

		// emission
		h_n = search_by_hash(hash_tree_texture, entries_mem[i].hash_emission);
		if (h_n)
			curr_mat->texture_emission_id = h_n->texture->id;
		else
			dbgprint("Materials", "Not found match with texture hash %x and material hash %x \n", entries_mem[i].hash_emission, curr_mat->mat_hash);


		add_hash_and_mat(curr_mat, curr_mat->mat_hash);
		i++;
	}

	CloseHandle(file);
	return 1;
}

int Read_materials(const wchar_t* MARE_folder_path)
{
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind = FindFirstFile(MARE_folder_path, &FindFileData);
	if (hFind == INVALID_HANDLE_VALUE) {
		dbgprint("Materials", "FindFirstFile failed (%d) %ws\n", GetLastError(), MARE_folder_path);
		return 0;
	}
	do {
		if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			continue;
		}
		else
		{
			Read_MARE(FindFileData.cFileName);
		}
	} while (FindNextFile(hFind, &FindFileData) != 0);

	FindClose(hFind);
	return 1;
}

DWORD Command_dump_model(DWORD* args)
{
	char* str = *(char**)args;

	if (!strncmp("all", str, 3))
	{
		Export(g_selected_model_index, false, false);
		return 0;
	}

	if (!strncmp("only_model", str, 10))
	{
		Export(g_selected_model_index, true, false);
		return 0;
	}

	if (!strncmp("only_current_anim", str, 17))
	{
		Export(g_selected_model_index, false, true);
		return 0;
	}

	
	return 0;
}