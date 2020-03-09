global Buffer_ID dashboard_buffer_id = -1;
global Face_ID dashboard_title_face = 0;

global String_Const_u8 dashboard_buffer_title = string_u8_litexpr("*dashboard*");

function void
dashboard_init_title_face(Application_Links *app) {
    Face_ID face = get_face_id(app, 0);
    Face_Description face_description = get_face_description(app, face);
    face_description.parameters.pt_size *= 5;
    dashboard_title_face = try_create_new_face(app, &face_description);
    if(dashboard_title_face == 0) {
        dashboard_title_face = face;
    }
}

function void
close_all_files(Application_Links *app) {
	Buffer_ID buffer = get_buffer_next(app, 0, Access_Always);
	while (buffer != 0) {
		buffer_kill(app, buffer, BufferKill_AlwaysKill);
		buffer = get_buffer_next(app, buffer, Access_Always);
	}
}

function void
draw_line_above_group(Application_Links *app, Text_Layout_ID text_layout_id, Face_ID face_id, char *search_str) {
    i64 buffer_size = buffer_get_size(app, dashboard_buffer_id);
    i64 pos = 0;

    i64 new_pos = 0;
    seek_string_insensitive_forward(app, dashboard_buffer_id, pos, 0, SCu8(search_str), &new_pos);
    if(new_pos < buffer_size) {
        Face_Metrics metrics = get_face_metrics(app, face_id);
        f32 line_height = metrics.line_height;

        Rect_f32 comment_first_char_rect = text_layout_character_on_screen(app, text_layout_id, new_pos);
        f32 line_center = comment_first_char_rect.y0 - (line_height / 2);

        Rect_f32 rect = {
            comment_first_char_rect.x0,
            line_center + 1,
            10000,
            line_center,
        };

        f32 roundness = 4.f;
        draw_rectangle(app, rect, roundness, fcolor_resolve(fcolor_id(defcolor_comment)));
    }
}

function void
move_to_str(Application_Links *app, char *search_str) {
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
    if(!buffer_exists(app, buffer)) {
        return;
    }

    i64 buffer_size = buffer_get_size(app, buffer);
    i64 pos = 0;

    i64 new_pos = 0;
    seek_string_insensitive_forward(app, buffer, pos, 0, SCu8(search_str), &new_pos);
    if(new_pos < buffer_size) {
        pos = new_pos;
    }

    if(pos != 0) {
        view_set_cursor_and_preferred_x(app, view, seek_pos(pos));
    }
}

function void
default_view_input_handler_dashboard(Application_Links *app, User_Input input) {
    // NOTE(allen): Get the binding from the buffer's current map
    View_ID view = get_this_ctx_view(app, Access_Always);
    Buffer_ID buffer = view_get_buffer(app, view, Access_Always);
    Managed_Scope buffer_scope = buffer_get_managed_scope(app, buffer);
    Command_Map_ID *map_id_ptr = scope_attachment(app, buffer_scope, buffer_map_id, Command_Map_ID);
    if(*map_id_ptr == 0) {
        *map_id_ptr = mapid_file;
    }
    Command_Map_ID map_id = *map_id_ptr;
    Command_Binding binding = map_get_binding_recursive(&framework_mapping, map_id, &input.event);
    Managed_Scope scope = view_get_managed_scope(app, view);

    if (binding.custom == 0){
        // NOTE(allen): we don't have anything to do with this input,
        // leave it marked unhandled so that if there's a follow up
        // event it is not blocked.
        leave_current_input_unhandled(app);
    }
    else{
        // NOTE(allen): before the command is called do some book keeping
        Rewrite_Type *next_rewrite =
            scope_attachment(app, scope, view_next_rewrite_loc, Rewrite_Type);
        *next_rewrite = Rewrite_None;
        if (fcoder_mode == FCoderMode_NotepadLike){
            for (View_ID view_it = get_view_next(app, 0, Access_Always);
                 view_it != 0;
                 view_it = get_view_next(app, view_it, Access_Always)){
                Managed_Scope scope_it = view_get_managed_scope(app, view_it);
                b32 *snap_mark_to_cursor =
                    scope_attachment(app, scope_it, view_snap_mark_to_cursor,
                                     b32);
                *snap_mark_to_cursor = true;
            }
        }

        // NOTE(allen): call the command
        binding.custom(app);

        next_rewrite = scope_attachment(app, scope, view_next_rewrite_loc, Rewrite_Type);
        if(next_rewrite != 0) {
            Rewrite_Type *rewrite = scope_attachment(app, scope, view_rewrite_loc, Rewrite_Type);
            *rewrite = *next_rewrite;
            if(fcoder_mode == FCoderMode_NotepadLike) {
                for(View_ID view_it = get_view_next(app, 0, Access_Always);
                    view_it != 0;
                    view_it = get_view_next(app, view_it, Access_Always)) {
                    Managed_Scope scope_it = view_get_managed_scope(app, view_it);
                    b32 *snap_mark_to_cursor = scope_attachment(app, scope_it, view_snap_mark_to_cursor, b32);
                    if(*snap_mark_to_cursor) {
                        i64 pos = view_get_cursor_pos(app, view_it);
                        view_set_mark(app, view_it, seek_pos(pos));
                    }
                }
            }
        }
    }
}

function void
load_project_from_dashboard_item(Application_Links *app, String_Const_u8 file_or_project) {
    u64 pos = 0;
    Range_i64 range = {};
    if(file_or_project.str[pos] == '\t') {
        ++pos;
    }

    range.first = pos;
    range.one_past_last = file_or_project.size;

    String_Const_u8 sc = string_substring(file_or_project, range);

    set_hot_directory(app, sc);
    close_all_files(app);
    load_project(app);
}

function b32
attempt_action_for_dashboard_item(Application_Links *app, i64 cursor_pos_arg) {
    Scratch_Block scratch(app);
    View_ID view_id = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer_id = view_get_buffer(app, view_id, Access_ReadVisible);

    Buffer_Cursor cursor = view_compute_cursor(app, view_id, seek_pos(cursor_pos_arg));

    String_Const_u8 file_or_project = push_buffer_line(app, scratch, buffer_id, cursor.line);
    b32 result = false;
    for(i64 i = cursor.line; i > 0; --i) {
        String_Const_u8 current_line = push_buffer_line(app, scratch, buffer_id, i);
        if(string_compare(current_line, string_u8_litexpr("Recent files: (r)")) == 0) {
            u64 pos = 0;
            Range_i64 range = {};
            if(file_or_project.str[pos] == '\t') {
                ++pos;
            }
            range.first = pos;
            range.one_past_last = file_or_project.size - 1; // TODO: need to check for file ending

            String_Const_u8 file = string_substring(file_or_project, range);
            Buffer_Create_Flag flags = BufferCreate_NeverNew;
            Buffer_ID buffer = create_buffer(app, file, flags);
            if(buffer != 0) {
                view_set_buffer(app, view_id, buffer, 0);
            }
            result = true;
            break;
        }

        else if(string_compare(current_line, string_u8_litexpr("Bookmarks: (m)")) == 0) {
            u64 pos = 0;
            Range_i64 range = {};
            while(file_or_project.str[pos] != '-') {
                ++pos;
            }
            range.first = pos + 2; // Skip past '-' and whitespace
            range.one_past_last = file_or_project.size - 1; // TODO: need to check for file ending

            String_Const_u8 file = string_substring(file_or_project, range);
            Buffer_Create_Flag flags = BufferCreate_NeverNew;
            Buffer_ID buffer = create_buffer(app, file, flags);
            if(buffer != 0) {
                view_set_buffer(app, view_id, buffer, 0);
            }
            result = true;
            break;
        }

        else if(string_compare(current_line, string_u8_litexpr("Projects: (p)")) == 0) {
            load_project_from_dashboard_item(app, file_or_project);
            result = true;
            break;
        }
    }

    return result;
}

function inline void
move_to_first_item_in_group(Application_Links *app, char *group) {
    move_to_str(app, group);
    move_down(app);
    move_right(app);
}

function void
command_override_for_dashboard_view(Application_Links *app) {
    View_ID view_id = get_active_view(app, Access_Always);

    User_Input in = {};
    for(;;) {
        in = get_next_input(app,
                            EventPropertyGroup_Any,
                            EventProperty_ViewActivation);

        b32 handled = true;
        switch(in.event.kind) {
            case InputEventKind_KeyStroke:
            {
                switch(in.event.key.code) {
                    case KeyCode_R: { move_to_first_item_in_group(app, "Recent files: (r)"); } break;
                    case KeyCode_M: { move_to_first_item_in_group(app, "Bookmarks: (m)"); } break;
                    case KeyCode_P: { move_to_first_item_in_group(app, "Projects: (p)"); } break;
                    case KeyCode_A: { move_to_first_item_in_group(app, "Agenda for today: (a)"); } break;

                    case KeyCode_Return: {
                        i64 cursor_pos = view_get_cursor_pos(app, view_id);
                        attempt_action_for_dashboard_item(app, cursor_pos);
                    } break;

                    default: { handled = false; } break;
                }
            } break;

            case InputEventKind_MouseButton:
            {
                switch (in.event.mouse.code){
                    case MouseCode_Left:
                    {
                    } break;

                    default: { handled = false; } break;
                }
            } break;

            default: { handled = false; } break;
        }

        if(!handled) {
            // Use the default command from the user's command map
            // that isn't intercepted above
            default_view_input_handler_dashboard(app, in);
        }
    }
}

function String_Const_u8
file_dump_dashboard(Arena *arena, char *name) {
    String_Const_u8 text = {};
    FILE *file = fopen(name, "rb");
    if (file != 0){
        fseek(file, 0, SEEK_END);
        text.size = ftell(file);
        fseek(file, 0, SEEK_SET);
        text.str = push_array(arena, u8, text.size + 1);
        if (text.str == 0){
            fprintf(stdout, "fatal error: not enough memory in partition for file dumping");
            exit(1);
        }
        fread(text.str, 1, (size_t)text.size, file);
        text.str[text.size] = 0;
        fclose(file);
    }

    return(text);
}

void
draw_dashboard_extras(Application_Links* app, Text_Layout_ID text_layout_id, Face_ID face_id, Rect_f32 rect) {
    draw_line_above_group(app, text_layout_id, face_id, "Recent files: (r)");
    draw_line_above_group(app, text_layout_id, face_id, "Bookmarks: (m)");
    draw_line_above_group(app, text_layout_id, face_id, "Projects: (p)");
    draw_line_above_group(app, text_layout_id, face_id, "Agenda for today: (a)");

    Face_Metrics metrics = get_face_metrics(app, dashboard_title_face);
    f32 adv = metrics.normal_advance;

    Face_Metrics n_metrics = get_face_metrics(app, face_id);
    f32 line_h = n_metrics.line_height;

    draw_string_oriented(app,
                         dashboard_title_face,
                         fcolor_resolve(fcolor_id(defcolor_cursor)),
                         string_u8_litexpr("4Coder"),
                         V2f32((rect.x1 / 2) - (adv * 3),
                               line_h + (line_h / 2)),
                         0,
                         V2f32(1.0f, 0.0f));
}

function void
load_dashboard_group(Application_Links *app, Arena *arena, char *name, char *group_title) {
    String_Const_u8 list = file_dump_dashboard(arena, name);
    write_text(app, SCu8(group_title));
    write_text(app, list);
    write_text(app, string_u8_litexpr("\n"));
}

CUSTOM_COMMAND_SIG(dashboard_open)
CUSTOM_DOC("Open new dashboard buffer.")
{
    View_ID vid = get_active_view(app, Access_Always);
    Buffer_Create_Flag flags = BufferCreate_AlwaysNew;
    Buffer_ID new_buffer = create_buffer(app, dashboard_buffer_title, flags);

    Scratch_Block scratch(app);

    if(new_buffer != 0) {
        dashboard_buffer_id = new_buffer;
        view_set_buffer(app, vid, new_buffer, 0);

        write_text(app, string_u8_litexpr("\n\n\n\n\n\n\n\n")); // space padding for logo

        load_dashboard_group(app, scratch, "dashboard/recent", "Recent files: (r)\n");
        load_dashboard_group(app, scratch, "dashboard/bookmarks", "Bookmarks: (m)\n");
        load_dashboard_group(app, scratch, "dashboard/project", "Projects: (p)\n");
        load_dashboard_group(app, scratch, "dashboard/agenda", "Agenda for today: (a)\n");

        buffer_set_setting(app, new_buffer, BufferSetting_ReadOnly, true);

        // Prevent 4coder from thinking it needs to save this buffer
        buffer_set_dirty_state(app, new_buffer, DirtyState_UpToDate);

        goto_beginning_of_file(app);
        move_to_first_item_in_group(app, "Recent files: (r)");

        i64 cursor_pos = view_get_cursor_pos(app, vid);
        view_set_mark(app, vid, seek_pos(cursor_pos));

        dashboard_init_title_face(app);
        command_override_for_dashboard_view(app);
    }
}
