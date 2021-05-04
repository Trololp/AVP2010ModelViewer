#include "stdafx.h"
#include "Keyboard.h"
#include "Console.h"
#include "Debug.h"

extern XMMATRIX	g_View;
extern float	pos_x;
extern float	pos_y;
extern float	pos_z;
extern float	g_alpha;
extern float	g_beta;
extern Console*	g_pConsole;
extern DWORD	g_loaded_mdl_count;
extern DWORD	g_selected_model_index;
extern DWORD	g_selected_anim_index;
extern DWORD    g_loaded_anim_count;
extern DWORD    g_anim_setup_bones;
extern bool g_animate;
extern bool g_show_node_pts;
extern bool g_show_HSSB;
extern bool g_show_skl;
extern bool g_show_HMPT;

//Move Directions 
bool b_Forward = false;
bool b_Backward = false;
bool b_Right = false;
bool b_Left = false;
bool b_RMB = false; // USE hold RMB mouse to look up
bool b_LMB = false;
bool b_Shift = false;
bool b_show_info = true;
bool b_Right_arrow = false;
bool b_Left_arrow = false;
bool b_Up_arrow = false;
bool b_Down_arrow = false;
bool b_Space = false;

// keys last state
bool b_Right_arrow_state = false;
bool b_Left_arrow_state = false;
bool b_Up_arrow_state = false;
bool b_Down_arrow_state = false;
bool b_Space_state = false;


inline bool FlipFlAp_func(bool val, bool* last_state)
{
	if (val != *last_state)
	{
		*last_state = val;
		if(val)
			return true;
	}
	else
	{
		return false;
	}
	return false;
}

void Specific_keys()
{
	b_Shift = (GetKeyState(VK_SHIFT) & 0x8000) ? true : false;
	b_Space = (GetKeyState(VK_SPACE) & 0x8000) ? true : false;
	b_Right_arrow = (GetKeyState(VK_RIGHT) & 0x8000) ? true : false;
	b_Left_arrow = (GetKeyState(VK_LEFT) & 0x8000) ? true : false;
	b_Up_arrow = (GetKeyState(VK_UP) & 0x8000) ? true : false;
	b_Down_arrow = (GetKeyState(VK_DOWN) & 0x8000) ? true : false;

	//Right arrow key
	if (FlipFlAp_func(b_Right_arrow, &b_Right_arrow_state))
	{
		g_selected_model_index = (g_selected_model_index + 1 < g_loaded_mdl_count) ? g_selected_model_index + 1 : g_selected_model_index;
		g_anim_setup_bones = 1;
	}
		

	//Left arrow key
	if (FlipFlAp_func(b_Left_arrow, &b_Left_arrow_state))
	{
		g_selected_model_index = (g_selected_model_index > 0) ? g_selected_model_index - 1 : g_selected_model_index;
		g_anim_setup_bones = 1;
	}
		

	//up arrow key
	if (FlipFlAp_func(b_Up_arrow, &b_Up_arrow_state))
	{
		g_selected_anim_index = (g_selected_anim_index + 1 < g_loaded_anim_count) ? g_selected_anim_index + 1 : g_selected_anim_index;
		//g_anim_setup_bones = 1;
	}
		

	//down arrow key
	if (FlipFlAp_func(b_Down_arrow, &b_Down_arrow_state))
	{
		g_selected_anim_index = (g_selected_anim_index > 0) ? g_selected_anim_index - 1 : g_selected_anim_index;
		//g_anim_setup_bones = 1;
	}
	
	// Space
	if (FlipFlAp_func(b_Space, &b_Space_state))
	{
		g_animate = !g_animate;
	}

}


void MovementFunc()
{
	Specific_keys();

	if (b_Forward || b_Backward || b_Left || b_Right)
	{
		float g_SpeedMultiplier = 10.0f * (1.0f - b_Shift * 0.9f);
		float delta_x = g_SpeedMultiplier * cos(g_alpha) * cos(g_beta);
		float delta_y = g_SpeedMultiplier * sin(g_alpha) * cos(g_beta);
		float delta_z = g_SpeedMultiplier * sin(g_beta);

		if (b_Forward)
		{
			pos_x -= delta_x * 0.1f;
			pos_y -= delta_y * 0.1f;
			pos_z -= delta_z * 0.1f;
		}
		if (b_Backward)
		{
			pos_x += delta_x * 0.1f;
			pos_y += delta_y * 0.1f;
			pos_z += delta_z * 0.1f;
		}
		if (b_Left)
		{
			pos_x -= delta_y * 0.1f;
			pos_y += delta_x * 0.1f;
		}
		if (b_Right)
		{
			pos_x += delta_y * 0.1f;
			pos_y -= delta_x * 0.1f;
		}

		XMVECTOR Eye = XMVectorSet(pos_y, pos_z, pos_x, 0.0f);
		XMVECTOR At = XMVectorSet(pos_y + delta_y, pos_z + delta_z, pos_x + delta_x, 0.0f);
		XMVECTOR Up = XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);
		g_View = XMMatrixLookAtLH(Eye, At, Up);
	}
}


void KeyDown(WPARAM wParam, LPARAM lParam)
{
	
	if (g_pConsole->is_active)
	{
		g_pConsole->Console_input(wParam, 1);
		return;
	}
		
	switch (wParam)
	{
	case 'W':
		b_Forward = true;
		break;
	case 'S':
		b_Backward = true;
		break;
	case 'A':
		b_Left = true;
		break;
	case 'D':
		b_Right = true;
		break;
	case 'H':
		b_show_info = b_show_info ? 0 : 1;
		break;
	case 'K':
		g_show_skl = g_show_skl ? 0 : 1;
		break;
	case 'P':
		g_pConsole->is_active = true;
		break;
	case 'V':
		g_show_HMPT = g_show_HMPT ? 0 : 1;
		break;
	case 'N':
		g_show_node_pts = g_show_node_pts ? 0 : 1;
		break;
	case 'B':
		g_show_HSSB = g_show_HSSB ? 0 : 1;
		break;
	default:
		break;
	}
}

void KeyChar(WPARAM wParam, LPARAM lParam)
{
	if (g_pConsole->is_active)
	{
		g_pConsole->Console_input(wParam, 0);
		return;
	}
}

void KeyUp(WPARAM wParam, LPARAM lParam)
{
	switch (wParam)
	{
	case 'W':
		b_Forward = false;
		break;
	case 'S':
		b_Backward = false;
		break;
	case 'A':
		b_Left = false;
		break;
	case 'D':
		b_Right = false;
		break;
	default:
		break;
	}
}

int x_diff = 0;
int y_diff = 0;
int last_x = 0;
int last_y = 0;

void MouseRMB(bool down, WPARAM wParam, LPARAM lParam)
{
	b_RMB = down;
	int xPos = LOWORD(lParam);
	int yPos = HIWORD(lParam);
	if (down)
	{
		x_diff += xPos - last_x;
		y_diff += yPos - last_y;
	}
	else
	{
		last_x = xPos;
		last_y = yPos;
	}
}

bool Show_info()
{
	return b_show_info;
}

void MouseLookUp(WPARAM wParam, LPARAM lParam)
{
	if (b_RMB)
	{
		int xPos = LOWORD(lParam);
		int yPos = HIWORD(lParam);

		int Dx = xPos - x_diff;
		int Dy = -yPos + y_diff;
		g_alpha = Dx / 100.0f;
		g_beta = Dy / 100.0f;

		if (g_beta >= XM_PIDIV2 - 0.1f)
			g_beta = XM_PIDIV2 - 0.1f;
		if (g_beta <= -XM_PIDIV2 + 0.1f)
			g_beta = -XM_PIDIV2 + 0.1f;

		float g_SpeedMultiplier = 10.0f * (1.0f - b_Shift * 0.9f);
		float delta_x = g_SpeedMultiplier * cos(g_alpha) * cos(g_beta);
		float delta_y = g_SpeedMultiplier * sin(g_alpha) * cos(g_beta);
		float delta_z = g_SpeedMultiplier * sin(g_beta);

		XMVECTOR Eye = XMVectorSet(pos_y, pos_z, pos_x, 0.0f);
		XMVECTOR At = XMVectorSet(pos_y + delta_y, pos_z + delta_z, pos_x + delta_x, 0.0f);
		XMVECTOR Up = XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);
		g_View = XMMatrixLookAtLH(Eye, At, Up);
	}
}

