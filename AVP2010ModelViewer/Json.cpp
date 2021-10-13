//---------------------------------------------------------------------------//
// The most bad json encoder & parser you ever see...                        //
// Designed for .gltf parsing but can be used somewhere else                 //
// By Trololp (2021)                                                         //
//---------------------------------------------------------------------------//

//#include <assert.h>
#include "stdafx.h"
#include "Json.h"
#include "Debug.h"

//globals
UINT g_token_index = 0;
std::vector <json_token> g_123;
std::vector <json_token> &g_current_token_vector = g_123;
json_token g_null_json_token((char*)"null", TOKEN_NULL, 0);

// Predefinitions
json_token* json_parse_field(std::vector<json_token> &v_tokens, char* p_start, char* p_end);
json_token* json_parse_array(char* array_name, std::vector<json_token> &v_tokens, char* p_start, char* p_end);
json_token* json_parse_object(char* obj_name, std::vector<json_token> &v_tokens, char* p_start, char* p_end);

json_parser::json_parser()
{
	//m_json_tokens.push_back(json_token((char*)"root", TOKEN_OBJ, 0));


}


json_parser::~json_parser()
{
	for (json_token t : m_json_tokens)
	{
		//if (t.pData && t.type == TOKEN_STR)
		//	free(t.pData);
		//if (t.p_field_name)
		//	free(t.p_field_name);
	}


	std::vector<json_token>().swap(m_json_tokens);
	//delete &m_json_tokens;
}

// skip ':' symbol
inline char* skip_separator(char* _p)
{
	char* p = _p;

	while (isspace(*p)) { p--; }
	p--; // :
	while (isspace(*p)) { p--; }
	return p;
}

// malloc string so must be free after...
// unsafe to infinity loop
// end_pp points to ..." quotation symbol
inline char* read_quot_str(char* p_start, char** end_pp)
{

	char* p_f_name_start = p_start + 1;
	char* p = p_start + 1;
	while (*p != '\"' || *(p - 1) == '\\') { p++; }
	*end_pp = p;										//  v  v
	char* p_f_name = (char*)malloc(p - p_f_name_start + 1); // "abc"
	strncpy(p_f_name, p_f_name_start, p - p_f_name_start);
	p_f_name[p - p_f_name_start] = '\0';


	return p_f_name;
}

// *p = '{' then return p to '}'
// *p = '[' then return pointer to ']'
// with hierarchy ofc
char* json_find_other_end(char* p)
{
	int layer = 0;
	char* p_ = p;

	switch (*p_)
	{
	case '[': while (*p_ != ']' || layer != 0) { if (*p_ == '[') layer++;  p_++; if (*p_ == ']') layer--; } break; // so move forward
	case '{': while (*p_ != '}' || layer != 0) { if (*p_ == '{') layer++;  p_++; if (*p_ == '}') layer--; } break;
	case ']': while (*p_ != '[' || layer != 0) { if (*p_ == ']') layer++;  p_--; if (*p_ == '[') layer--; } break; // and backward
	case '}': while (*p_ != '{' || layer != 0) { if (*p_ == '}') layer++;  p_--; if (*p_ == '{') layer--; } break;

	default:
		break;
	}

	return p_;

}


// p_start                     p_end
// v                           v
// {"a":"abc", "b":1.23, "c":{}}
json_token* json_parse_object(char* obj_name, std::vector<json_token> &v_tokens, char* p_start, char* p_end)
{
	char* p = p_end - 1;

	UINT obj_fields_count = 0;
	std::vector <UINT> obj_fields_ids;

	while (p_start != p)
	{
		// checking for spaces at the end
		while (isspace(*p)) { p--; }

		json_token_type type = TOKEN_NUMBER;
		switch (*p)
		{
		case '\"': type = TOKEN_STR; break; // "str"
		case 'l': type = TOKEN_NULL; break; // null
		case 'e': type = TOKEN_BOOL; break; // true false
		case ']': type = TOKEN_ARRAY; break;
		case '}': type = TOKEN_OBJ; break;
		}

		if (type == TOKEN_NUMBER)
		{
			char* p_num = p;

			while (isdigit(*p_num) || *p_num == 'e' || *p_num == '-' || *p_num == '+') { p_num--; }

			if (*p_num == '.')
				type = TOKEN_FLOAT_NUMBER;
		}

		// find field start
		char* p_field_end = p;

		switch (type)
		{
		case TOKEN_NULL:
		{			//     v
					//p -= 6; //  "..a":null
			p -= 4;
			p = skip_separator(p);
		}
		break;
		case TOKEN_OBJ:
		{
			p = json_find_other_end(p); // skips object
										//    v  v
			p -= 1; // "..a":{...}
			p = skip_separator(p);
		}
		break;
		case TOKEN_STR:
		{
			p--;
			while (*p != '\"' || *(p - 1) == '\\') p--;
			//    v  v
			p -= 1; // "..a":"..."
			p = skip_separator(p);
		}
		break;
		case TOKEN_FLOAT_NUMBER:
		case TOKEN_NUMBER:
		{
			while (*p != ':') p--;
			p--;
			while (isspace(*p)) { p--; }
		}
		break;
		case TOKEN_BOOL:
		{								  // v     v
			p -= *(p - 1) == 'u' ? 4 : 5; // a":true
			p = skip_separator(p);
		}
		break;
		case TOKEN_ARRAY:
		{
			p = json_find_other_end(p); // skips array
										//    v  v
			p -= 1; // "..a":[...]
			p = skip_separator(p);

		}
		break;
		default:
			dbgprint("json_parse_object", "Something wrong with object parser, something wrong with field parser, hit default: branch \n");
			return nullptr;
		}

		p--;
		while (*p != '\"' || *(p - 1) == '\\') p--;


		json_parse_field(v_tokens, p, p_field_end);
		obj_fields_ids.push_back(g_token_index - 1);

		p--;
		while (isspace(*p)) { p--; }

		obj_fields_count++;

		if (*p == ',')
		{
			p--; // shift to new member
			continue; // while loop continue... 
		}

	}

	for (UINT j = 0; j < obj_fields_count; j++)
	{
		json_token* p_curr_token = new json_token((char*)"obj_tbl", TOKEN_OBJ_PTR, g_token_index++);
		p_curr_token->set_count(obj_fields_ids[j]);
		v_tokens.push_back(*p_curr_token);
	}

	json_token* obj_token = new json_token(obj_name, TOKEN_OBJ, g_token_index++);
	obj_token->set_count(obj_fields_count);
	v_tokens.push_back(*obj_token);

	// this object
	return &(v_tokens.at(g_token_index - 1));
}

// p_start           p_end
// v                 v
// ["a", "b", "c"... ]
json_token* json_parse_array(char* array_name, std::vector<json_token> &v_tokens, char* p_start, char* p_end)
{
	char* p = p_end;
	p--;

	UINT arr_members_count = 0;
	std::vector <UINT> arr_mbrs_ids;


	while (p_start != p)
	{
		// checking for spaces at the end
		while (isspace(*p)) { p--; }

		json_token_type type = TOKEN_NUMBER;
		switch (*p)
		{
		case '\"': type = TOKEN_STR; break; // "str"
		case 'l': type = TOKEN_NULL; break; // null
		case 'e': type = TOKEN_BOOL; break; // true false
		case ']': type = TOKEN_ARRAY; break;
		case '}': type = TOKEN_OBJ; break;
		}

		if (type == TOKEN_NUMBER)
		{
			char* p_num = p;

			while (isdigit(*p_num) || *p_num == 'e' || *p_num == '-' || *p_num == '+') { p_num--; }

			if (*p_num == '.')
				type = TOKEN_FLOAT_NUMBER;
		}

		json_token* p_curr_token = new json_token((char*)"array_member", type, g_token_index);

		switch (type)
		{
		case TOKEN_NULL:
		{
			p -= 4; // p now should be on comma or p_start (braket)
			p_curr_token->set_count(0);
			v_tokens.push_back(*p_curr_token);
			g_token_index++;
		}
		break;
		case TOKEN_OBJ:
		{
			// parse obj code
			char* p_obj_start = json_find_other_end(p);
			json_parse_object((char*)"obj_inside_array", v_tokens, p_obj_start, p);
			p = p_obj_start - 1;

		}
		break;
		case TOKEN_STR:
		{
			char* p_str_start;
			p--;
			char* ch_readed;

			while (*p != '\"' || *(p - 1) == '\\') p--; // skips escape sequence \" and reach start of string
			p_str_start = p;

			char* p_val_str = read_quot_str(p_str_start, &ch_readed);

			p_curr_token->set_str(p_val_str);

			p--;
			v_tokens.push_back(*p_curr_token);
			g_token_index++;
		}
		break;
		case TOKEN_NUMBER:
		{
			char* p_num_start;
			char* p_num_end;

			while (*p != ',' && *p != '[') p--; // reach comma or braket
			p_num_start = p + 1;
			int num = strtol(p_num_start, &p_num_end, 10);

			p_curr_token->set_int(num);
			v_tokens.push_back(*p_curr_token);
			g_token_index++;
		}
		break;

		case TOKEN_FLOAT_NUMBER:
		{
			char* p_num_start;
			char* p_num_end;

			while (*p != ',' && *p != '[') p--; // reach comma or braket
			p_num_start = p + 1;
			float num = strtof(p_num_start, &p_num_end);

			p_curr_token->set_float(num);
			v_tokens.push_back(*p_curr_token);
			g_token_index++;
		}
		break;

		case TOKEN_BOOL:
		{
			bool val = *(p - 1) == 'u' ? 1 : 0;
			p -= *(p - 1) == 'u' ? 4 : 5; // true/false
			p_curr_token->set_bool(val);
			v_tokens.push_back(*p_curr_token);
			g_token_index++;
		}
		break;
		case TOKEN_ARRAY:
		{
			char* p_array_start = json_find_other_end(p);
			json_parse_array((char*)"array_inside_array", v_tokens, p_array_start, p);
			p = p_array_start - 1; // comma or braket
		}
		break;
		default:
			dbgprint("json_parse_array", "Something wrong with array parser, something wrong with field parser, hit default: branch\n");
			return nullptr;
		}

		arr_mbrs_ids.push_back(g_token_index - 1);
		arr_members_count++;
		//g_token_index++;
		while (isspace(*p)) { p--; }

		if (*p == ',')
		{
			p--; // shift to new member
		}

	}

	for (UINT j = 0; j < arr_members_count; j++)
	{
		json_token* p_curr_token = new json_token((char*)"array_tbl", TOKEN_ARRAY_PTR, g_token_index++);
		p_curr_token->set_count(arr_mbrs_ids[j]);
		v_tokens.push_back(*p_curr_token);
	}

	json_token* arr_token = new json_token(array_name, TOKEN_ARRAY, g_token_index++);
	arr_token->set_count(arr_members_count);
	v_tokens.push_back(*arr_token);

	// this array
	return &(v_tokens.at(g_token_index - 1));
}

// start   end
// v       v
// "abc":"a",
json_token* json_parse_field(std::vector<json_token> &v_tokens, char* p_start, char* p_end)
{
	json_token_type type = TOKEN_NUMBER;

	// checking for spaces at the end
	while (isspace(*p_end)) { p_end--; }

	switch (*p_end)
	{
	case '\"': type = TOKEN_STR; break; // "str"
	case 'l': type = TOKEN_NULL; break; // null
	case 'e': type = TOKEN_BOOL; break; // true false
	case ']': type = TOKEN_ARRAY; break;
	case '}': type = TOKEN_OBJ; break;
	}

	if (type == TOKEN_NUMBER)
	{
		char* p_num = p_end;

		while (isdigit(*p_num) || *p_num == 'e' || *p_num == '-' || *p_num == '+') { p_num--; }

		if (*p_num == '.')
			type = TOKEN_FLOAT_NUMBER;
	}

	// read field name						  v
	char* p_field_name_end = p_start + 1; // "a.." points to 'a'
	while (*p_field_name_end != '\"' || *(p_field_name_end - 1) == '\\')
	{
		p_field_name_end++;
	}
	p_field_name_end--;

	char* p_val;
	char* p_f_name = read_quot_str(p_start, &p_val);

	p_val += 1;

	// skip ':'
	while (isspace(*p_val)) { p_val++; }
	p_val++;
	while (isspace(*p_val)) { p_val++; }

	json_token* p_field_token = new json_token(p_f_name, type, g_token_index);

	switch (type)
	{
	case TOKEN_NULL:
		// simple nothing to do
		v_tokens.push_back(*p_field_token);
		g_token_index++;
		break;
	case TOKEN_OBJ:
		// call to parse object code
	{
		char* p_obj_start = json_find_other_end(p_end);
		json_parse_object(p_f_name, v_tokens, p_obj_start, p_end);
	}
	break;

	case TOKEN_STR:
	{
		char* val_str_end_p;
		char* val_str = read_quot_str(p_val, &val_str_end_p);
		if (val_str_end_p != p_end)
		{
			dbgprint("json_parse_field", "parsing str error \n");
			return nullptr;
		}
		p_field_token->set_str(val_str);
		v_tokens.push_back(*p_field_token);
		g_token_index++;
	}
	break;
	case TOKEN_NUMBER:
	{
		char* val_str_end_p;
		char* val_str = p_val;
		int num = strtol(val_str, &val_str_end_p, 10);

		p_field_token->set_int(num);
		v_tokens.push_back(*p_field_token);
		g_token_index++;
	}
	break;
	case TOKEN_FLOAT_NUMBER:
	{
		char* val_str_end_p;
		char* val_str = p_val;
		float num = strtof(val_str, &val_str_end_p);

		p_field_token->set_float(num);
		v_tokens.push_back(*p_field_token);
		g_token_index++;
	}
	break;
	case TOKEN_BOOL:
	{											//   v     v
		bool val = *(p_end - 1) == 'u' ? 1 : 0; // true false
		p_field_token->set_bool(val);
		v_tokens.push_back(*p_field_token);
		g_token_index++;
	}
	break;
	case TOKEN_ARRAY:
	{
		char* p_array_start = json_find_other_end(p_end);
		json_parse_array(p_f_name, v_tokens, p_array_start, p_end);
	}
	break;
	default:
		dbgprint("json_parse_field", "something wrong with field parser, hit default: branch\n");
		return nullptr;
	}

	//g_token_index++;

	// this field
	return &(v_tokens.at(g_token_index - 1));
}

int json_parser::json_parse(char * str)
{
	if (!str)
	{
		dbgprint("json_parser::json_parse", "no str ptr \n");
		return 0;
	}

	m_pJSON_str = str;
	UINT json_str_len = strlen(str);

	g_token_index = 0;

	if (*str != '{' && !isspace(*str))
	{
		dbgprint("json_parser::json_parse", "Some junk at the start, instead of json\n");
		return 0;
	}

	char* p_start = str;
	while (*p_start != '{') p_start++;

	char* p = str + json_str_len - 1;
	while (*p != '}') p--;

	// this gona end badly

	m_root_token = json_parse_object((char*)"root", m_json_tokens, p_start, p);
	g_current_token_vector = m_json_tokens;
	m_amount_tokens = g_token_index;

	if (m_root_token)
		return 1;

	return 0;
}

UINT json_parser::estimate_member_size(json_token * mbr_token, bool is_obj)
{

	UINT field_name_size = is_obj ? 3 + strlen(mbr_token->p_field_name) : 0;
	char num_buffer[50] = { 0 }; // float values can be this size ("-340282346638528859811704183484516925440.000")

	switch (mbr_token->type)
	{
	case TOKEN_NULL:
		return field_name_size + sizeof("null") - 1;
	case TOKEN_OBJ:
		return field_name_size + json_estimate_arr_or_obj_size(mbr_token);
	case TOKEN_STR:
		return field_name_size + 2 + strlen(mbr_token->get_str());
	case TOKEN_FLOAT_NUMBER:
		sprintf(num_buffer, "%.3f", mbr_token->get_float());
		return field_name_size + strlen(num_buffer);
	case TOKEN_NUMBER:
		sprintf(num_buffer, "%d", mbr_token->get_int());
		return field_name_size + strlen(num_buffer);
	case TOKEN_BOOL:
		return field_name_size + mbr_token->get_bool() ? sizeof("true") - 1 : sizeof("false") - 1;
	case TOKEN_ARRAY:
		return field_name_size + json_estimate_arr_or_obj_size(mbr_token);
	default:
		dbgprint(__FUNCTION__, "incorrect token encountered (%d)\n", mbr_token->type);
		return 0;
		break;
	}

}

UINT json_parser::json_estimate_arr_or_obj_size(json_token* objarr_token)
{
	UINT cnt = objarr_token->get_count();
	UINT commas_num = cnt > 1 ? cnt - 1 : 0;

	bool is_obj = objarr_token->type == TOKEN_OBJ;

	UINT mbrs_size = 0;

	for (UINT i = 0; i < cnt; i++)
	{
		json_token* mbr = get_array_member(objarr_token, i);
		mbrs_size += estimate_member_size(mbr, is_obj);
	}

	return mbrs_size + 2 + commas_num;
}


UINT json_parser::json_estimate_serialized_str_size()
{
	if (!m_root_token || m_json_tokens.size() == 0)
		return 0;

	if (m_root_token->type != json_token_type::TOKEN_OBJ)
	{
		dbgprint(__FUNCTION__, "is this a correct json format ?, it not starts as object \n");
		return 0;
	}

	return json_estimate_arr_or_obj_size(m_root_token);
}

// make sure that dst contain buffer with size larger than 
// json_estimate_serialized_str_size() value
int json_parser::json_serialize(char * dst)
{
	if (!m_root_token || m_json_tokens.size() == 0 || !dst)
		return 0;

	if (m_root_token->type != json_token_type::TOKEN_OBJ)
	{
		dbgprint(__FUNCTION__, "is this a correct json format ?, it not starts as object \n");
		return 0;
	}

	m_ch_ptr = dst;
	char* start_val = m_ch_ptr;

	if (!print_arr_or_obj(m_root_token))
	{
		dbgprint(__FUNCTION__, "something went wrong !\n");
		return 0;
	}

	*m_ch_ptr = '\0';

	return m_ch_ptr - start_val; // lenght
}

UINT json_parser::print_arr_or_obj(json_token * objarr_token)
{
	bool is_obj = objarr_token->type == TOKEN_OBJ;

	//strncpy(m_ch_ptr, is_obj ? "{" : "[", 1);
	*m_ch_ptr = is_obj ? '{' : '[';
	m_ch_ptr++;

	UINT cnt = objarr_token->get_count();

	for (UINT i = 0; i < cnt; i++)
	{
		if (!print_member(get_array_member(objarr_token, i), is_obj))
			return 0;
		if (i + 1 < cnt)
		{
			*m_ch_ptr = ',';
			m_ch_ptr++;
		}
	}

	*m_ch_ptr = is_obj ? '}' : ']';
	m_ch_ptr++;

	return 1;
}

UINT json_parser::print_member(json_token * mbr_token, bool is_obj)
{
	const char* fmt_str = "format_error";



	if (mbr_token->type == TOKEN_OBJ || mbr_token->type == TOKEN_ARRAY)
	{
		if (is_obj)
		{
			UINT len = strlen(mbr_token->p_field_name) + 4;
			snprintf(m_ch_ptr, len, "\"%s\":", mbr_token->p_field_name);
			m_ch_ptr += len - 1;
		}
		print_arr_or_obj(mbr_token);
		return 1;
	}

	UINT mbr_size = estimate_member_size(mbr_token, is_obj) + 1;


	switch (mbr_token->type)
	{
	case TOKEN_NULL:
		if (is_obj)
			snprintf(m_ch_ptr, mbr_size, "\"%s\":null", mbr_token->p_field_name);
		else
			snprintf(m_ch_ptr, mbr_size, "null");
		break;
	case TOKEN_STR:
		if (is_obj)
			snprintf(m_ch_ptr, mbr_size, "\"%s\":\"%s\"", mbr_token->p_field_name, mbr_token->get_str());
		else
			snprintf(m_ch_ptr, mbr_size, "\"%s\"", mbr_token->get_str());
		break;
	case TOKEN_FLOAT_NUMBER:
		if (is_obj)
			snprintf(m_ch_ptr, mbr_size, "\"%s\":%.3f", mbr_token->p_field_name, mbr_token->get_float());
		else
			snprintf(m_ch_ptr, mbr_size, "%.3f", mbr_token->get_float());
		break;
	case TOKEN_NUMBER:
		if (is_obj)
			snprintf(m_ch_ptr, mbr_size, "\"%s\":%d", mbr_token->p_field_name, mbr_token->get_int());
		else
			snprintf(m_ch_ptr, mbr_size, "%d", mbr_token->get_int());
		break;
	case TOKEN_BOOL:
		if (mbr_token->get_bool())
		{
			if (is_obj)
				snprintf(m_ch_ptr, mbr_size, "\"%s\":true", mbr_token->p_field_name);
			else
				snprintf(m_ch_ptr, mbr_size, "true");
		}
		else
		{
			if (is_obj)
				snprintf(m_ch_ptr, mbr_size, "\"%s\":false", mbr_token->p_field_name);
			else
				snprintf(m_ch_ptr, mbr_size, "false");
		}

		break;
	default:
		dbgprint(__FUNCTION__, "incorrect token encountered (%d)\n", mbr_token->type);
		return 0;
	}

	//snprintf(m_ch_ptr, mbr_size, fmt_str, mbr_token->p_field_name);


	m_ch_ptr += mbr_size - 1;

	return 1;
}


json_token * json_parser::get_field(json_token * from, const char * str)
{
	if (from == NULL)
	{
		if (m_json_tokens.size() < 2)
		{
			dbgprint("json_parser::get_field", "m_json_tokens.size() less than 2 !!!\n");
			return nullptr;
		}

		from = m_json_tokens.data();
	}


	if (from->type != TOKEN_OBJ)
	{
		dbgprint("json_parser::get_field", "this token is not an object\n");
		return nullptr;
	}
	UINT cnt = from->get_count();
	UINT id = from->id + 1; // next elem

	for (UINT i = 0; i < cnt; i++)
	{
		if (m_json_tokens[id].type != TOKEN_OBJ_PTR)
		{
			dbgprint("json_parser::get_field", "error obj pointer table incomplete\n");
			return nullptr;
		}
		UINT id_obj_mbr = m_json_tokens[id].get_count();

		id++;

		if (id_obj_mbr >= m_json_tokens.size())
		{
			dbgprint("json_parser::get_field", "error obj pointer out of range !\n");
			return nullptr;
		}

		if (!strcmp(m_json_tokens[id_obj_mbr].p_field_name, str))
		{
			return &(m_json_tokens.at(id_obj_mbr));
		}

	}

	//dbgprint("json_parser::get_field", "No field with name: \"%s\" was found in obj \"%s\" !\n", str, from->p_field_name);
	return nullptr;
}

json_token * json_parser::get_array_member(json_token* arr_token, UINT num)
{
	if (!arr_token)
	{
		dbgprint("json_parser::get_array_member", "arr_token == nullptr \n");
		return nullptr;
	}

	if (arr_token->get_count() <= num)
	{
		dbgprint("json_parser::get_array_member", "index out of range \n");
		return nullptr;
	}

	UINT id = arr_token->id + 1 + num;
	UINT arr_mbr_id = m_json_tokens[id].get_count();

	if (arr_mbr_id >= m_json_tokens.size())
	{
		dbgprint("json_parser::get_array_member", "error array pointer out of range !\n");
		return nullptr;
	}

	return &(m_json_tokens.at(arr_mbr_id));
}

json_token * json_parser::create_obj_or_arr(UINT amount_elems, const char* name, bool is_obj)
{
	UINT obj_index = g_token_index;
	m_json_tokens.push_back(*(new json_token((char*)name, is_obj ? TOKEN_OBJ : TOKEN_ARRAY, g_token_index++)));

	m_json_tokens[obj_index].set_count(amount_elems);
	m_json_tokens[obj_index].last_unused_ptr = g_token_index;

	for (UINT i = 0; i < amount_elems; i++)
	{
		m_json_tokens.push_back(*(new json_token((char*)"ptr", is_obj ? TOKEN_OBJ_PTR : TOKEN_ARRAY_PTR, g_token_index)));
		m_json_tokens[g_token_index].set_count(obj_index); // all points at obj
		g_token_index++;
	}

	return &(m_json_tokens.at(obj_index));
}

json_token* json_parser::add_obj_or_arr_member(UINT obj_token_id, json_token_type type, const char* name, UINT amount_elems)
{
	json_token* obj_token =&(m_json_tokens.at(obj_token_id));
	bool is_obj = obj_token->type == TOKEN_OBJ;

	UINT last_ptr = obj_token->id + obj_token->get_count();

	if (obj_token->get_count() == 0 || obj_token->last_unused_ptr > last_ptr)
	{
		dbgprint(__FUNCTION__, "excided object or array capacity! \n");
		return nullptr;
	}

	UINT current_unset_pointer = obj_token->last_unused_ptr;
	obj_token->last_unused_ptr++;

	json_token * p_arr_or_obj = nullptr;

	switch (type)
	{
		// This data is 1 element long
	case TOKEN_NULL:
	case TOKEN_STR:
	case TOKEN_FLOAT_NUMBER:
	case TOKEN_NUMBER:
	case TOKEN_BOOL:
		m_json_tokens.push_back(*(new json_token(is_obj ? (char*)name : (char*)"arr_mbr", type, g_token_index)));
		m_json_tokens[current_unset_pointer].set_count(g_token_index);
		g_token_index++;
		return &(m_json_tokens.at(g_token_index - 1));

	case TOKEN_OBJ:
	case TOKEN_ARRAY:
		p_arr_or_obj = create_obj_or_arr(amount_elems, is_obj ? (char*)name : (char*)"arr_mbr", type == TOKEN_OBJ);
		m_json_tokens[current_unset_pointer].set_count(p_arr_or_obj->id);
		return p_arr_or_obj;

	default:
		dbgprint(__FUNCTION__, "incorrect token type %d \n", type);
		return nullptr;
	}

}

void json_parser::flush_data()
{
	std::vector<json_token>().swap(m_json_tokens);
	m_root_token = nullptr;
	g_token_index = 0;
	return;
}

void json_parser::test_dump()
{
	int i = 0;

	for (json_token token : m_json_tokens)
	{
		switch (token.type)
		{
		case TOKEN_NULL:      dbgprint("json_parser::test_dump", "%d (%s) %s %08x\n", i, token.p_field_name, "TOKEN_NULL", token.pData); break;
		case TOKEN_OBJ:       dbgprint("json_parser::test_dump", "%d (%s) %s %d\n", i, token.p_field_name, "TOKEN_OBJ", token.pData); break;
		case TOKEN_OBJ_PTR:	  dbgprint("json_parser::test_dump", "%d (%s) %s %d\n", i, token.p_field_name, "TOKEN_OBJ_PTR", token.pData); break;
		case TOKEN_STR:		  dbgprint("json_parser::test_dump", "%d (%s) %s %s\n", i, token.p_field_name, "TOKEN_STR", token.pData); break;
		case TOKEN_NUMBER:	  dbgprint("json_parser::test_dump", "%d (%s) %s %f\n", i, token.p_field_name, "TOKEN_NUMBER", token.get_float()); break;
		case TOKEN_BOOL:	  dbgprint("json_parser::test_dump", "%d (%s) %s %08x\n", i, token.p_field_name, "TOKEN_BOOL", token.pData);	break;
		case TOKEN_ARRAY:	  dbgprint("json_parser::test_dump", "%d (%s) %s %d\n", i, token.p_field_name, "TOKEN_ARRAY", token.pData);	break;
		case TOKEN_ARRAY_PTR: dbgprint("json_parser::test_dump", "%d (%s) %s %d\n", i, token.p_field_name, "TOKEN_ARRAY_PTR", token.pData); break;
		default:
			dbgprint("json_parser::test_dump", "%s %08x\n", "ERROR", token.pData);
			break;
		}
		i++;
	}
}

json_token json_token::operator[](const char * str)
{
	json_token* from = this;

	if (this == NULL)
	{
		if (g_current_token_vector.size() < 2)
		{
			dbgprint(__FUNCTION__, "m_json_tokens.size() less than 2 !!!\n");
			return g_null_json_token;
		}

		from = g_current_token_vector.data();
	}


	if (from->type != TOKEN_OBJ)
	{
		dbgprint(__FUNCTION__, "this token is not an object\n");
		return g_null_json_token;
	}
	UINT cnt = from->get_count();
	UINT id = from->id - 1; // next elem

	for (UINT i = 0; i < cnt; i++)
	{
		if (g_current_token_vector[id].type != TOKEN_OBJ_PTR)
		{
			dbgprint(__FUNCTION__, "error obj pointer table incomplete\n");
			return g_null_json_token;
		}
		UINT id_obj_mbr = g_current_token_vector[id].get_count();


		if (id_obj_mbr >= g_current_token_vector.size() || id_obj_mbr > id--)
		{
			dbgprint(__FUNCTION__, "error obj pointer out of range !\n");
			return g_null_json_token;
		}

		if (!strcmp(g_current_token_vector[id_obj_mbr].p_field_name, str))
		{
			return g_current_token_vector.at(id_obj_mbr);
		}

	}
	return g_null_json_token;
}

json_token json_token::operator[](int num)
{
	if (!this)
	{
		dbgprint(__FUNCTION__, "arr_token == nullptr \n");
		return g_null_json_token;
	}

	if (this->get_count() <= num)
	{
		dbgprint(__FUNCTION__, "index out of range \n");
		return g_null_json_token;
	}

	UINT id = this->id - 1 - num;
	UINT arr_mbr_id = g_current_token_vector[id].get_count();

	if (arr_mbr_id >= g_current_token_vector.size() || arr_mbr_id > id)
	{
		dbgprint(__FUNCTION__, "error array pointer out of range !\n");
		return g_null_json_token;
	}

	return g_current_token_vector.at(arr_mbr_id);
}
