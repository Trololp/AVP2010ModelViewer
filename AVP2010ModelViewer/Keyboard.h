#pragma once
#include "MainClass_Viewer_Screen.h"

void MovementFunc();

void KeyDown(WPARAM wParam, LPARAM lParam);
void KeyChar(WPARAM wParam, LPARAM lParam);
void KeyUp(WPARAM wParam, LPARAM lParam);
void MouseLookUp(WPARAM wParam, LPARAM lParam);
void MouseRMB(bool down, WPARAM wParam, LPARAM lParam);
bool Show_info();