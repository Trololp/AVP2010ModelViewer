#include "stdafx.h"
#include "Drawable_info.h"
#include "Debug.h"
#include "Models.h"

extern ID3D11Device*           g_pd3dDevice;
extern ID3D11Buffer*		   g_pVertexBuffer_generic_bbox;
extern ID3D11Buffer*			g_pVertexBuffer_lines;
extern ID3D11DeviceContext*    g_pImmediateContext;
extern std::vector <bbox>		g_bboxes;
extern DWORD g_loaded_mdl_count;
//const int max_models = 1000;
extern model_incremental_info g_mmi[1000];
extern XMMATRIX g_axBoneMatrices[128];





void Reqursive_update_lines_skeleton(model_skeleton* skl, DWORD index, XMVECTOR prev_pos, std::vector<line> &skl_lines)
{
	XMMATRIX bone_matrix = skl->pBones[index].bone_model_matrix;
	XMVECTOR bone_pos = bone_matrix.r[3];

	XMVECTOR pos = XMVector3Transform(bone_pos, g_axBoneMatrices[index]);//g_axBoneMatrices[index].r[3];//  M.r[3] + 
	//pos += prev_pos;

	XMFLOAT3 pos1;
	XMFLOAT3 pos2;

	XMStoreFloat3(&pos1, prev_pos);
	XMStoreFloat3(&pos2, pos );
	if(index != 0 )
		skl_lines.push_back({ pos1, pos2,{ 1.0f, 0.0f, 0.0f, 1.0f } }); // not draw root bone

	int i = 1;
	do
	{
		if (skl->pBones[i].parent_bone_id == index)
			Reqursive_update_lines_skeleton(skl, i, pos, skl_lines);
		i++;
	} while (i < skl->amount_bones);


	return;
}

int Draw_info_Update_skeleton_lines2(model_skeleton* skl, ID3D11Buffer* buffr, simple_vertex* vrts)
{
	if (!skl || !buffr || !vrts)
		return 0;

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));

	std::vector<line> skl_lines;
	Reqursive_update_lines_skeleton(skl, 0, g_XMZero, skl_lines);

	HRESULT hr = S_OK;


	int lines_count = skl_lines.size();
	int size_of_vertex_data = 2 * sizeof(simple_vertex) * lines_count;

	int r = 0;

	for (std::vector<line>::iterator it = skl_lines.begin(); it != skl_lines.end(); ++it)
	{
		XMFLOAT4 clr = (*it).Color;
		XMFLOAT3 p1 = (*it).p1;
		XMFLOAT3 p2 = (*it).p2;

		vrts[0 + r] = { p1, clr }; // (p1) ---- (p2)
		vrts[1 + r] = { p2, clr }; //
		r += 2;
	}

	g_pImmediateContext->Map(buffr, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	memcpy(mappedResource.pData, vrts, size_of_vertex_data);
	g_pImmediateContext->Unmap(buffr, 0);

	return 0;
}

void Reqursive_make_lines_skeleton(model_skeleton* skl, DWORD index, XMVECTOR prev_pos, std::vector<line> &skl_lines)
{
	XMMATRIX M = skl->pBones[index].bone_model_matrix;
	XMVECTOR pos = M.r[3];

	XMFLOAT3 pos1;
	XMFLOAT3 pos2;

	XMStoreFloat3(&pos1, prev_pos);
	XMStoreFloat3(&pos2, pos);
	if (index != 0)
		skl_lines.push_back({ pos1, pos2,{ 1.0f, 0.0f, 0.0f, 1.0f } });

	int i = 1;
	do
	{
		if (skl->pBones[i].parent_bone_id == index)
			Reqursive_make_lines_skeleton(skl, i, pos, skl_lines);
		i++;
	} while (i < skl->amount_bones);


	return;
}



void Make_skeleton_lines(model_skeleton* skl, ID3D11Buffer** buffer, DWORD* skl_points, simple_vertex** vertices)
{
	std::vector<line> skl_lines;
	Reqursive_make_lines_skeleton(skl, 0, g_XMZero, skl_lines);
	line_to_vertex_to_buffer(skl_lines, skl_points, buffer, vertices);
}

void Make_static_skeleton_lines()
{
	for (int i = 0; i < g_loaded_mdl_count; i++)
	{
		if (g_mmi[i].skl)
		{
			DWORD points = 0;
			ID3D11Buffer* buff_p = nullptr;
			simple_vertex* buff_v = nullptr;
			Make_skeleton_lines(g_mmi[i].skl, &buff_p, &points, &buff_v);
			//dbgprint("Skeleton", "ptr test %08x \n", buff_p);
			g_mmi[i].points_skeleton = points;
			g_mmi[i].dynamic_skeleton_buffer = buff_p;
			g_mmi[i].dynamic_skeleton_points = buff_v;
		}
	}
}

void Call_1(struct1* a)
{
	Draw_info_Update_skeleton_lines2((model_skeleton*)(a->info1), (ID3D11Buffer*)(a->info2), (simple_vertex*)(a->info3));
}
