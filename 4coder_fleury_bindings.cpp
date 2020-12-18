//~ NOTE(rjf): Bindings


CUSTOM_COMMAND_SIG(ryanb_search) {
    Scratch_Block scratch(app);
    
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
    i64 buffer_size = buffer_get_size(app, buffer);
    
    i64 pos = view_get_cursor_pos(app, view);
    String_Const_u8 query_init = push_token_or_word_under_active_cursor(app, scratch);
    
    Scan_Direction scan = Scan_Forward;
    
    Query_Bar_Group group(app);
    Query_Bar bar = {};
    if (start_query_bar(app, &bar, 0) == 0) {
        return;
    }
    
    u8 bar_string_space[256];
    bar.string = SCu8(bar_string_space, query_init.size);
    block_copy(bar.string.str, query_init.str, query_init.size);
    bar.prompt = string_u8_litexpr("Search: ");
    
    b32 do_clear_query = true;
    u64 match_size = bar.string.size;
    
    User_Input in = { };
    for (;;) {
        isearch__update_highlight(app, view, Ii64_size(pos, match_size));
        center_view(app);
        
        in = get_next_input(app, EventPropertyGroup_AnyKeyboardEvent, EventProperty_Escape|EventProperty_ViewActivation|EventProperty_MouseButton);
        if (in.abort) {
            break;
        }
        
        String_Const_u8 string = to_writable(&in);
        
        b32 string_change = false;
        
        // allow paste
        if (match_key_code(&in, KeyCode_V)) {
            Input_Modifier_Set* mods = &in.event.key.modifiers;
            if (has_modifier(mods, KeyCode_Control)) {
                String_Const_u8 paste = push_clipboard_index(app, scratch, 0, 0);
                if (paste.size > 0) {
                    bar.string.size = 0;
                    String_u8 bar_string = Su8(bar.string, sizeof(bar_string_space));
                    string_append(&bar_string, paste);
                    bar.string = bar_string.string;
                    string_change = true;
                }
            }
        }
        
        if (string.str != 0 && string.size > 0) {
            if (do_clear_query) {
                do_clear_query = false;
                if (bar.string.size > 0) {
                    bar.string.size = 0;
                    string_change = true;
                }
            }
            String_u8 bar_string = Su8(bar.string, sizeof(bar_string_space));
            string_append(&bar_string, string);
            bar.string = bar_string.string;
            string_change = true;
        }
        else if (match_key_code(&in, KeyCode_Backspace)) {
            if (do_clear_query) {
                do_clear_query = false;
                if (bar.string.size > 0) {
                    bar.string.size = 0;
                    string_change = true;
                }
            }
            else if (is_unmodified_key(&in.event)) {
                u64 old_bar_string_size = bar.string.size;
                bar.string = backspace_utf8(bar.string);
                string_change = (bar.string.size < old_bar_string_size);
            }
            else if (has_modifier(&in.event.key.modifiers, KeyCode_Control)) {
                if (bar.string.size > 0){
                    bar.string.size = 0;
                    string_change = true;
                }
            }
        }
        
        b32 do_scan_action = false;
        Scan_Direction change_scan = scan;
        if (!string_change) {
            if (match_key_code(&in, KeyCode_Return) || match_key_code(&in, KeyCode_Tab)) {
                //if (match_key_code(&in, KeyCode_F3)) {// || match_key_code(&in, KeyCode_Tab)) {
                Input_Modifier_Set* mods = &in.event.key.modifiers;
                do_scan_action = true;
                
                if (has_modifier(mods, KeyCode_Shift)) {
                    change_scan = Scan_Backward;
                }
                else {
                    change_scan = Scan_Forward;
                }
            }
        }
        
        if (string_change){
            switch (scan){
                case Scan_Forward: {
                    i64 new_pos = 0;
                    seek_string_insensitive_forward(app, buffer, pos - 1, 0, bar.string, &new_pos);
                    if (new_pos < buffer_size){
                        pos = new_pos;
                        match_size = bar.string.size;
                    }
                }
                break;
                
                case Scan_Backward: {
                    i64 new_pos = 0;
                    seek_string_insensitive_backward(app, buffer, pos + 1, 0, bar.string, &new_pos);
                    if (new_pos >= 0){
                        pos = new_pos;
                        match_size = bar.string.size;
                    }
                }
                break;
            }
        }
        else if (do_scan_action) {
            scan = change_scan;
            switch (scan){
                case Scan_Forward: {
                    i64 new_pos = 0;
                    seek_string_insensitive_forward(app, buffer, pos, 0, bar.string, &new_pos);
                    if (new_pos < buffer_size){
                        pos = new_pos;
                        match_size = bar.string.size;
                    }
                }
                break;
                
                case Scan_Backward: {
                    i64 new_pos = 0;
                    seek_string_insensitive_backward(app, buffer, pos, 0, bar.string, &new_pos);
                    if (new_pos >= 0){
                        pos = new_pos;
                        match_size = bar.string.size;
                    }
                }
                break;
            }
        }
        else{
            leave_current_input_unhandled(app);
        }
    }
    
    view_disable_highlight_range(app, view);
    view_set_cursor_and_preferred_x(app, view, seek_pos(pos));
}

static Key_Code
F4_MapStringToKeyCode(String_Const_u8 key_string)
{
    Key_Code result = 0;
    for(int i = 1; i < KeyCode_COUNT; i += 1)
    {
        String_Const_u8 str = {};
        str.data = key_code_name[i];
        str.size = strlen(key_code_name[i]);
        if(string_match(str, key_string))
        {
            result = i;
            break;
        }
    }
    return result;
}

static b32
F4_LoadBindingsFromFile(Application_Links *app, Mapping *mapping, String_Const_u8 filename)
{
    b32 result = 0;
    
    Scratch_Block scratch(app);
    MappingScope();
    SelectMapping(mapping);
    
    char *filename_cstr = push_array(scratch, char, filename.size+1);
    MemoryCopy(filename_cstr, filename.str, filename.size);
    filename_cstr[filename.size] = 0;
    FILE *file = open_file_try_current_path_then_binary_path(app, filename_cstr);
    if(file != 0)
    {
        Data data = dump_file_handle(scratch, file);
        Config *parsed = config_from_text(app, scratch, filename, SCu8(data));
        
        if(parsed)
        {
            result = 1;
            
            struct
            {
                String_Const_u8 name;
                Managed_ID id;
            }
            maps[] = 
            {
                { string_u8_litexpr("keys_global"), mapid_global },
                { string_u8_litexpr("keys_file"),   mapid_file   },
                { string_u8_litexpr("keys_code"),   mapid_code   },
            };
            
            for(int map_idx = 0; map_idx < ArrayCount(maps); map_idx += 1)
            {
                SelectMap(maps[map_idx].id);
                
                Config_Compound *compound = 0;
                if(config_compound_var(parsed, maps[map_idx].name, 0, &compound))
                {
                    Config_Get_Result_List list = typed_compound_array_reference_list(scratch, parsed, compound);
                    for(Config_Get_Result_Node *node = list.first; node != 0; node = node->next)
                    {
                        Config_Compound *src = node->result.compound;
                        String_Const_u8 cmd_string = {0};
                        String_Const_u8 key_string = {0};
                        String_Const_u8 mod_string[4] = {0};
                        
                        if(!config_compound_string_member(parsed, src, "cmd", 0, &cmd_string))
                        {
                            config_add_error(scratch, parsed, node->result.pos, "Command string is required in binding");
                            goto finish_map;
                        }
                        
                        if(!config_compound_string_member(parsed, src, "key", 1, &key_string))
                        {
                            config_add_error(scratch, parsed, node->result.pos, "Key string is required in binding");
                            goto finish_map;
                        }
                        
                        for(int mod_idx = 0; mod_idx < ArrayCount(mod_string); mod_idx += 1)
                        {
                            String_Const_u8 str = push_stringf(scratch, "mod_%i", mod_idx);
                            if(config_compound_string_member(parsed, src, str, 2 + mod_idx, &mod_string[mod_idx]))
                            {
                                // NOTE(rjf): No-Op
                            }
                        }
                        
                        // NOTE(rjf): Map read in successfully.
                        {
                            
                            // NOTE(rjf): Find command.
                            Command_Metadata *command = 0;
                            {
                                for(int i = 0; i < ArrayCount(fcoder_metacmd_table); i += 1)
                                {
                                    Command_Metadata *candidate = fcoder_metacmd_table + i;
                                    String_Const_u8 str = { candidate->name, (u64)candidate->name_len };
                                    if(string_match(str, cmd_string))
                                    {
                                        command = candidate;
                                        break;
                                    }
                                }
                            }
                            
                            // NOTE(rjf): Find keycode.
                            Key_Code keycode = F4_MapStringToKeyCode(key_string);
                            
                            // NOTE(rjf): Find mods.
                            int mod_count = 0;
                            Key_Code mods[ArrayCount(mod_string)] = {0};
                            for(int i = 0; i < ArrayCount(mod_string); i += 1)
                            {
                                if(mod_string[i].str)
                                {
                                    mods[mod_count] = F4_MapStringToKeyCode(mod_string[i]);
                                    mod_count += 1;
                                }
                            }
                            
                            if(keycode && command)
                            {
                                Input_Modifier_Set mods_set = { mods, mod_count, };
                                map_set_binding(mapping, map, command->proc, InputEventKind_KeyStroke, keycode, &mods_set);
                            }
                            else
                            {
                                config_add_error(scratch, parsed, node->result.pos, (char*)
                                                 (keycode ? "Invalid command" : ((command) ? "Invalid key": "Invalid command and key")));
                            }
                            
                        }
                        
                        finish_map:;
                    }
                }
                
                if(parsed && parsed->errors.first)
                {
                    String_Const_u8 error_text = config_stringize_errors(app, scratch, parsed);
                    print_message(app, error_text);
                }
            }
        }
    }
    
    return result;
}

static void
F4_SetAbsolutelyNecessaryBindings(Mapping *mapping)
{
    MappingScope();
    SelectMapping(mapping);
    
    SelectMap(mapid_global);
    BindCore(fleury_startup, CoreCode_Startup);
    BindCore(default_try_exit, CoreCode_TryExit);
    Bind(exit_4coder,          KeyCode_F4, KeyCode_Alt);
    BindMouseWheel(mouse_wheel_scroll);
    BindMouseWheel(mouse_wheel_change_face_size, KeyCode_Control);
    
    SelectMap(mapid_file);
    ParentMap(mapid_global);
    BindTextInput(fleury_write_text_input);
    BindMouse(click_set_cursor_and_mark, MouseCode_Left);
    BindMouseRelease(click_set_cursor, MouseCode_Left);
    BindCore(click_set_cursor_and_mark, CoreCode_ClickActivateView);
    BindMouseMove(click_set_cursor_if_lbutton);
    
    SelectMap(mapid_code);
    ParentMap(mapid_file);
    BindTextInput(fleury_write_text_and_auto_indent);
}

static void
F4_SetDefaultBindings(Mapping *mapping)
{
    MappingScope();
    SelectMapping(mapping);
    SelectMap(mapid_global);
    Bind(keyboard_macro_start_recording , KeyCode_U, KeyCode_Control);
    Bind(keyboard_macro_finish_recording, KeyCode_U, KeyCode_Control, KeyCode_Shift);
    Bind(keyboard_macro_replay,           KeyCode_U, KeyCode_Alt);
    Bind(change_active_panel,           KeyCode_W, KeyCode_Alt);
    Bind(change_active_panel_backwards, KeyCode_W, KeyCode_Alt, KeyCode_Shift);
    Bind(interactive_new,               KeyCode_N, KeyCode_Control);
    Bind(interactive_open_or_new,       KeyCode_O, KeyCode_Control);
    Bind(open_in_other,                 KeyCode_O, KeyCode_Alt, KeyCode_Shift);
    Bind(interactive_kill_buffer,       KeyCode_K, KeyCode_Control);
    Bind(interactive_switch_buffer,     KeyCode_I, KeyCode_Control);
    Bind(project_go_to_root_directory,  KeyCode_H, KeyCode_Control);
    Bind(save_all_dirty_buffers,        KeyCode_S, KeyCode_Control, KeyCode_Shift);
    Bind(change_to_build_panel,         KeyCode_Period, KeyCode_Alt);
    Bind(close_build_panel,             KeyCode_Period, KeyCode_Alt, KeyCode_Shift);
    Bind(goto_next_jump,                KeyCode_N, KeyCode_Alt);
    Bind(goto_prev_jump,                KeyCode_N, KeyCode_Alt, KeyCode_Shift);
    Bind(build_in_build_panel,          KeyCode_M, KeyCode_Alt);
    Bind(goto_first_jump,               KeyCode_M, KeyCode_Alt, KeyCode_Shift);
    Bind(toggle_filebar,                KeyCode_B, KeyCode_Alt);
    Bind(execute_any_cli,               KeyCode_Z, KeyCode_Alt);
    Bind(execute_previous_cli,          KeyCode_Z, KeyCode_Alt, KeyCode_Shift);
    Bind(command_lister,                KeyCode_X, KeyCode_Alt);
    Bind(project_command_lister,        KeyCode_X, KeyCode_Alt, KeyCode_Shift);
    Bind(list_all_functions_current_buffer_lister, KeyCode_I, KeyCode_Control, KeyCode_Shift);
    Bind(project_fkey_command, KeyCode_F1);
    Bind(project_fkey_command, KeyCode_F2);
    //Bind(project_fkey_command, KeyCode_F3);
    Bind(project_fkey_command, KeyCode_F4);
    Bind(project_fkey_command, KeyCode_F5);
    Bind(project_fkey_command, KeyCode_F6);
    Bind(project_fkey_command, KeyCode_F7);
    Bind(project_fkey_command, KeyCode_F8);
    Bind(project_fkey_command, KeyCode_F9);
    Bind(project_fkey_command, KeyCode_F10);
    Bind(project_fkey_command, KeyCode_F11);
    Bind(project_fkey_command, KeyCode_F12);
    Bind(project_fkey_command, KeyCode_F13);
    Bind(project_fkey_command, KeyCode_F14);
    Bind(project_fkey_command, KeyCode_F15);
    Bind(project_fkey_command, KeyCode_F16);
    
    // NOTE(rjf): Custom bindings.
    {
        Bind(open_panel_vsplit, KeyCode_P, KeyCode_Control);
        Bind(open_panel_hsplit, KeyCode_Minus, KeyCode_Control);
        Bind(close_panel, KeyCode_W, KeyCode_Control, KeyCode_Shift);
        Bind(fleury_place_cursor, KeyCode_Tick, KeyCode_Alt);
        //Bind(fleury_toggle_power_mode, KeyCode_P, KeyCode_Alt);
        Bind(jump_to_definition, KeyCode_J, KeyCode_Control);
        //Bind(fleury_toggle_battery_saver, KeyCode_Tick, KeyCode_Alt);
    }
    
    SelectMap(mapid_file);
    ParentMap(mapid_global);
    Bind(delete_char,            KeyCode_Delete);
    Bind(backspace_char,         KeyCode_Backspace);
    Bind(move_up,                KeyCode_Up);
    Bind(move_down,              KeyCode_Down);
    Bind(move_left,              KeyCode_Left);
    Bind(move_right,             KeyCode_Right);
    Bind(seek_end_of_line,       KeyCode_End);
    Bind(fleury_home,            KeyCode_Home);
    Bind(page_up,                KeyCode_PageUp);
    Bind(page_down,              KeyCode_PageDown);
    Bind(goto_beginning_of_file, KeyCode_Home, KeyCode_Control);
    Bind(goto_end_of_file,       KeyCode_End, KeyCode_Control);    
    Bind(move_up_to_blank_line_end,        KeyCode_Up, KeyCode_Control);
    Bind(move_down_to_blank_line_end,      KeyCode_Down, KeyCode_Control);
    Bind(move_left_whitespace_boundary,    KeyCode_Left, KeyCode_Control);
    Bind(move_right_whitespace_boundary,   KeyCode_Right, KeyCode_Control);
    Bind(move_line_up,                     KeyCode_Up, KeyCode_Alt);
    Bind(move_line_down,                   KeyCode_Down, KeyCode_Alt);
    Bind(backspace_alpha_numeric_boundary, KeyCode_Backspace, KeyCode_Control);
    Bind(delete_alpha_numeric_boundary,    KeyCode_Delete, KeyCode_Control);
    Bind(snipe_backward_whitespace_or_token_boundary, KeyCode_Backspace, KeyCode_Alt);
    Bind(snipe_forward_whitespace_or_token_boundary,  KeyCode_Delete, KeyCode_Alt);
    Bind(set_mark,                    KeyCode_Space, KeyCode_Control);
    Bind(replace_in_range,            KeyCode_A, KeyCode_Control);
    Bind(copy,                        KeyCode_C, KeyCode_Control);
    Bind(delete_range,                KeyCode_D, KeyCode_Control);
    Bind(delete_line,                 KeyCode_D, KeyCode_Control, KeyCode_Shift);
    Bind(center_view,                 KeyCode_E, KeyCode_Control);
    Bind(left_adjust_view,            KeyCode_E, KeyCode_Control, KeyCode_Shift);
    // Bind(search,                      KeyCode_F, KeyCode_Control);
    Bind(ryanb_search,                KeyCode_F, KeyCode_Control);
    Bind(list_all_locations,          KeyCode_F, KeyCode_Control, KeyCode_Shift);
    Bind(list_all_substring_locations_case_insensitive, KeyCode_F, KeyCode_Alt);
    Bind(goto_line,                   KeyCode_G, KeyCode_Control);
    Bind(list_all_locations_of_selection,  KeyCode_G, KeyCode_Control, KeyCode_Shift);
    //Bind(snippet_lister,              KeyCode_J, KeyCode_Control);
    
    Bind(kill_buffer,                 KeyCode_K, KeyCode_Control, KeyCode_Shift);
    Bind(kill_buffer,                 KeyCode_W, KeyCode_Control);
    
    Bind(duplicate_line,              KeyCode_L, KeyCode_Control);
    Bind(cursor_mark_swap,            KeyCode_M, KeyCode_Control);
    Bind(reopen,                      KeyCode_O, KeyCode_Control, KeyCode_Shift);
    Bind(query_replace,               KeyCode_Q, KeyCode_Control);
    Bind(query_replace_identifier,    KeyCode_Q, KeyCode_Control, KeyCode_Shift);
    Bind(query_replace_selection,     KeyCode_Q, KeyCode_Alt);
    Bind(reverse_search,              KeyCode_R, KeyCode_Control);
    Bind(save,                        KeyCode_S, KeyCode_Control);
    Bind(save_all_dirty_buffers,      KeyCode_S, KeyCode_Control, KeyCode_Shift);
    Bind(search_identifier,           KeyCode_T, KeyCode_Control);
    Bind(list_all_locations_of_identifier, KeyCode_T, KeyCode_Control, KeyCode_Shift);
    Bind(paste_and_indent,            KeyCode_V, KeyCode_Control);
    Bind(paste_next_and_indent,       KeyCode_V, KeyCode_Control, KeyCode_Shift);
    Bind(cut,                         KeyCode_X, KeyCode_Control);
    Bind(redo,                        KeyCode_Y, KeyCode_Control);
    Bind(undo,                        KeyCode_Z, KeyCode_Control);
    Bind(view_buffer_other_panel,     KeyCode_1, KeyCode_Control);
    Bind(swap_panels,                 KeyCode_2, KeyCode_Control);
    // Bind(if_read_only_goto_position,  KeyCode_Return);
    // Bind(if_read_only_goto_position_same_panel, KeyCode_Return, KeyCode_Shift);
    //Bind(view_jump_list_with_lister,  KeyCode_Period, KeyCode_Control, KeyCode_Shift);
    
    // NOTE(rjf): Custom bindings.
    {
        Bind(fleury_code_peek,          KeyCode_Alt, KeyCode_Control);
        Bind(fleury_close_code_peek,    KeyCode_Escape);
        // Bind(fleury_code_peek_go,       KeyCode_Return, KeyCode_Control);
        // Bind(fleury_code_peek_go_same_panel, KeyCode_Return, KeyCode_Control, KeyCode_Shift);
        Bind(fleury_write_zero_struct,  KeyCode_0, KeyCode_Control);
        Bind(fleury_smart_replace_identifier,     KeyCode_R, KeyCode_Control, KeyCode_Shift);
    }
    
    SelectMap(mapid_code);
    ParentMap(mapid_file);
    BindTextInput(fleury_write_text_and_auto_indent);
    Bind(move_left_alpha_numeric_boundary,           KeyCode_Left, KeyCode_Control);
    Bind(move_right_alpha_numeric_boundary,          KeyCode_Right, KeyCode_Control);
    Bind(move_left_alpha_numeric_or_camel_boundary,  KeyCode_Left, KeyCode_Alt);
    Bind(move_right_alpha_numeric_or_camel_boundary, KeyCode_Right, KeyCode_Alt);
    Bind(comment_line_toggle,        KeyCode_ForwardSlash, KeyCode_Control);
    Bind(word_complete,              KeyCode_Tab);
    Bind(auto_indent_range,          KeyCode_Tab, KeyCode_Control);
    Bind(auto_indent_line_at_cursor, KeyCode_Tab, KeyCode_Shift);
    Bind(word_complete_drop_down,    KeyCode_Tab, KeyCode_Shift, KeyCode_Control);
    Bind(write_block,                KeyCode_R, KeyCode_Alt);
    Bind(write_todo,                 KeyCode_T, KeyCode_Alt);
    Bind(write_note,                 KeyCode_Y, KeyCode_Alt);
    Bind(list_all_locations_of_type_definition,               KeyCode_D, KeyCode_Alt);
    Bind(list_all_locations_of_type_definition_of_identifier, KeyCode_T, KeyCode_Alt, KeyCode_Shift);
    Bind(open_long_braces,           KeyCode_LeftBracket, KeyCode_Control);
    Bind(open_long_braces_semicolon, KeyCode_LeftBracket, KeyCode_Control, KeyCode_Shift);
    Bind(open_long_braces_break,     KeyCode_RightBracket, KeyCode_Control, KeyCode_Shift);
    Bind(select_surrounding_scope,   KeyCode_LeftBracket, KeyCode_Alt);
    Bind(select_surrounding_scope_maximal, KeyCode_LeftBracket, KeyCode_Alt, KeyCode_Shift);
    Bind(select_prev_scope_absolute, KeyCode_RightBracket, KeyCode_Alt);
    Bind(select_prev_top_most_scope, KeyCode_RightBracket, KeyCode_Alt, KeyCode_Shift);
    Bind(select_next_scope_absolute, KeyCode_Quote, KeyCode_Alt);
    Bind(select_next_scope_after_current, KeyCode_Quote, KeyCode_Alt, KeyCode_Shift);
    Bind(place_in_scope,             KeyCode_ForwardSlash, KeyCode_Alt);
    Bind(delete_current_scope,       KeyCode_Minus, KeyCode_Alt);
    Bind(if0_off,                    KeyCode_I, KeyCode_Alt);
    Bind(open_file_in_quotes,        KeyCode_1, KeyCode_Alt);
    
    Bind(open_matching_file_cpp,     KeyCode_O, KeyCode_Alt);
    Bind(list_all_functions_current_buffer_lister, KeyCode_M, KeyCode_Alt);
}
