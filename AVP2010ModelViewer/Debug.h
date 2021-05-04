#pragma once
#include "MainClass_Viewer_Screen.h"
using namespace DirectX;

void dbgprint(const char* debug_note, const char* fmt, ...);
DWORD Console_command_tp(DWORD* args);
bbox* shifted_bbox_to_normal(XMVECTOR C, bbox* bounds, XMVECTOR quat);
int bbox_to_vertex(std::vector <bbox> &bboxes, DWORD* g_points);
int bbox_to_vertex_to_buffer(std::vector <bbox> &bboxes, DWORD* g_points, ID3D11Buffer** buffer);
int line_to_vertex(std::vector <line> &lines, DWORD* g_points);
int line_to_vertex_to_buffer(std::vector<line> &lines, DWORD* g_points, ID3D11Buffer** buffer, simple_vertex** vertices_buff);
void Dump_matrix(const char* name, XMMATRIX matrix);
void Dump_vector(const char* name, XMVECTOR vector);
void Dump_hex(const char* name, void* data, unsigned int count);
void Dump_floats(const char* name, void* data, unsigned int count);