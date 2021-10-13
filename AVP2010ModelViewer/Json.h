//---------------------------------------------------------------------------//
// The most bad json encoder & parser you ever see...                        //
// Designed for .gltf parsing but can be used somewhere else                 //
// By Trololp (2021)                                                         //
//---------------------------------------------------------------------------//
#pragma once
#include "MainClass_Viewer_Screen.h"
// JSON parser
static_assert(sizeof(float) == sizeof(void *), "sizeof(float) != sizeof(void *) type casting wont work here!");

enum json_token_type
{
	TOKEN_NULL,
	TOKEN_OBJ,
	TOKEN_OBJ_PTR, // points at object field
	TOKEN_STR,
	TOKEN_FLOAT_NUMBER,
	TOKEN_NUMBER,
	TOKEN_BOOL,
	TOKEN_ARRAY,
	TOKEN_ARRAY_PTR, // points at array field
};



struct json_token
{
	json_token_type type;
	UINT id;
	char* p_field_name;
	void* pData;
	UINT last_unused_ptr;

	json_token(char* name, json_token_type _type, UINT _id)
	{
		type = _type;
		id = _id;
		p_field_name = nullptr;
		pData = nullptr;

		if (name)
		{
			p_field_name = (char*)malloc(strlen(name) + 1);
			strcpy(p_field_name, name);
		}
	}


	// it appears that vector, while increasing it capacity deallocate old array
	// and cause to call destructor for this object
	// and so it free's memory for names... and crashes.
	/*
	~json_token()
	{
	if (p_field_name)
	{
	//dbgprint("~json_token", "%08x, \"%s\"\n", p_field_name, p_field_name);

	free(p_field_name);
	}

	if (pData && type == TOKEN_STR)
	free(pData);
	}*/

	char* get_str()
	{
		return (char*)(this->pData);
	}
	bool get_bool()
	{
		return (unsigned int)(this->pData) == 0 ? 0 : 1;
	}
	float get_float()
	{
		return *(float*)&(this->pData);
	}
	int get_int()
	{
		return (int)(this->pData);
	}


	// also used for ID extraction
	unsigned int get_count()
	{
		return (unsigned int)(this->pData);
	}

	void set_str(char* str) { this->pData = str; }
	void set_bool(bool b) { this->pData = (void*)b; }
	void set_int(int i) { this->pData = (void*)i; };
	void set_float(float f) {
		DWORD dw = *reinterpret_cast<DWORD const*>(&f);
		(this->pData) = (void*)dw;
	} // castmare
	void set_count(UINT ui) { this->pData = (void*)ui; }

	// this is made for simplicity mostly analogues of 
	// get_field(json_token* from, const char* str);
	// get_array_member(json_token* arr_token, UINT num);
	// class functions

	// str is a member name of object
	json_token operator[](const char* str);

	// num is id of array
	json_token operator[](int num);


};

class json_parser
{
public:


	char * m_pJSON_str = nullptr;
	unsigned int m_amount_tokens;

	std::vector <json_token> m_json_tokens;
	json_token* m_root_token = nullptr;

	json_parser();
	~json_parser();

	int json_parse(char* str);
	UINT json_estimate_serialized_str_size();
	int json_serialize(char* dst);


	json_token* get_field(json_token* from, const char* str);
	json_token* get_array_member(json_token* arr_token, UINT num);

	json_token* create_obj_or_arr(UINT amount_elems, const char* name, bool is_obj);
	//json_token* create_arr(UINT amount_elems, char* name);
	json_token* add_obj_or_arr_member(UINT obj_token_id, json_token_type type, const char * name, UINT amount_elems = 0);
	//void add_arr_member(json_token* obj_token, json_token_type type, UINT amount_elems = 0);

	void flush_data();

	void test_dump();
private:
	char* m_ch_ptr = nullptr;

	UINT estimate_member_size(json_token* mbr_token, bool is_obj);
	UINT json_estimate_arr_or_obj_size(json_token* objarr_token);

	UINT print_arr_or_obj(json_token* objarr_token);
	UINT print_member(json_token* mbr_token, bool is_obj);

};

