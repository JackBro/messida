// Copyright (C) 2015 Dr. MefistO
//
// This program is free software : you can redistribute it and / or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License 2.0 for more details.
//
// A copy of the GPL 2.0 should have been included with the program.
// If not, see http ://www.gnu.org/licenses/

#define VERSION "1.5.3"

#include <Windows.h>
#include <unordered_map>

#include <ida.hpp>
#include <dbg.hpp>
#include <idd.hpp>
#include <loader.hpp>

#undef interface

#include "emu.h"
#include "debugcpu.h"
#include "debugcon.h"

#include "mess_debmod.h"
#include "debug.h"

#include "exodus_helpers\WindowFunctions.h"
#include "exodus_windows\vdp\PaletteView\PaletteView.h"
#include "exodus_windows\vdp\PlaneView\PlaneView.h"
#include "exodus_windows\vdp\resource.h"

extern debugger_t debugger;
extern running_machine *g_running_machine;

static bool plugin_inited;
static bool dbg_started;

std::unordered_map<int, HWND> openedWindows;

static int idaapi hook_idp(void *user_data, int notification_code, va_list va)
{
	switch (notification_code)
	{
	case processor_t::custom_ana:
	{
		(*ph.u_ana)();

		op_t &op = cmd.Operands[0];

		switch (op.type)
		{
		case o_near:
		{
			if (op.phrase == 8 && op.specflag1 == 0)
			{
				op.type = o_mem;
				op.phrase = 9;
				return 2;
			}
			else if (op.phrase == 10 && op.specflag1 == 2) // with PC
			{
				op.type = o_displ;
				op.phrase = 91;
				cmd.Operands[1].type = o_void;
				return 2;
			}
		}
		}
	} break;
	}
	return 0;
}

#define EXODUS_MAIN_MENU "Exodus"
#define EXODUS_VDP_DEBUG_MENU "VDP Debug"

#define EXODUS_VDP_PALETTE_MENU "Palette"
#define EXODUS_VDP_PLANE_VIEWER_MENU "Plane Viewer"

bool check_window_opened(int id, std::unordered_map<int, HWND>::const_iterator *pair)
{
	std::unordered_map<int, HWND>::const_iterator found = openedWindows.find(id);
	
	if (pair != NULL)
		*pair = found;

	return found != openedWindows.end();
}

static bool idaapi create_exodus_vdp_palette_window(void *ud)
{
	std::unordered_map<int, HWND>::const_iterator found;
	if (check_window_opened(EXODUS_VDP_PALETTE_ID, &found))
	{
		SetForegroundWindow(found->second);
		return true;
	}
	
	//Set the window settings for this view
	static const unsigned int paletteRows = 4;
	static const unsigned int paletteColumns = 16;
	static const unsigned int paletteSquareDesiredSize = 15;
	int width = DPIScaleWidth(paletteSquareDesiredSize) * paletteColumns;
	int height = DPIScaleHeight(paletteSquareDesiredSize) * paletteRows;
	
	WNDCLASSEX wc;
	HWND hwnd;
	const char szClassName[] = "EXODUSVDPPALWND";

	//Step 1: Registering the Window Class
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = ExodusVdpPalWndProcWindow;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = GetHInstance();
	wc.hIcon = NULL;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = szClassName;
	wc.hIconSm = NULL;

	UnregisterClass(wc.lpszClassName, wc.hInstance);

	if (!RegisterClassEx(&wc))
	{
		MessageBox(NULL, "Window Registration Failed!", "Error!",
			MB_ICONEXCLAMATION | MB_OK);
		return false;
	}

	// Step 2: Creating the Window
	hwnd = CreateWindowEx(
		(WS_EX_TOOLWINDOW | WS_EX_STATICEDGE | WS_EX_APPWINDOW),
		szClassName,
		"Palette",
		(DS_SETFONT | DS_3DLOOK | DS_FIXEDSYS | WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME),
		CW_USEDEFAULT, CW_USEDEFAULT, width, height + DPIScaleHeight(30),
		NULL, NULL, wc.hInstance, NULL);

	if (hwnd == NULL)
	{
		MessageBox(NULL, "Window Creation Failed!", "Error!",
			MB_ICONEXCLAMATION | MB_OK);
		return false;
	}

	openedWindows.emplace(EXODUS_VDP_PALETTE_ID, hwnd);
	
	UpdateWindow(hwnd);

	return true;
}

static void install_exodus_vdp_vdp_palette_menu()
{
	if (dbg_started)
	{
		add_menu_item("Debugger/" /*EXODUS_MAIN_MENU "/" EXODUS_VDP_DEBUG_MENU*/,
			EXODUS_VDP_PALETTE_MENU,
			NULL,
			SETMENU_APP | SETMENU_CTXAPP,
			create_exodus_vdp_palette_window,
			NULL);
	}
}

static void remove_exodus_vdp_vdp_palette_menu()
{
	if (dbg_started)
	{
		del_menu_item("Debugger/" /*EXODUS_MAIN_MENU "/" EXODUS_VDP_DEBUG_MENU "/"*/ EXODUS_VDP_PALETTE_MENU);
	}
}

static bool idaapi create_exodus_vdp_plane_viewer_window(void *ud)
{
	std::unordered_map<int, HWND>::const_iterator found;
	if (check_window_opened(EXODUS_VDP_PLANE_VIEWER_ID, &found))
	{
		SetForegroundWindow(found->second);
		return true;
	}
	
	HWND hwnd = CreateDialog(GetHInstance(), MAKEINTRESOURCE(IDD_S315_5313_PLANEVIEW), NULL, ExodusVdpPlaneViewWndProcDialog);

	if (hwnd == NULL)
	{
		MessageBox(NULL, "Window Creation Failed!", "Error!",
			MB_ICONEXCLAMATION | MB_OK);
		return false;
	}

	openedWindows.emplace(EXODUS_VDP_PLANE_VIEWER_ID, hwnd);
	UpdateWindow(hwnd);

	return true;
}

static void install_exodus_vdp_plane_viewer_menu()
{
	if (dbg_started)
	{
		add_menu_item("Debugger/" /*EXODUS_MAIN_MENU "/" EXODUS_VDP_DEBUG_MENU*/,
			EXODUS_VDP_PLANE_VIEWER_MENU,
			NULL,
			SETMENU_APP | SETMENU_CTXAPP,
			create_exodus_vdp_plane_viewer_window,
			NULL);
	}
}

static void remove_exodus_vdp_plane_viewer_menu()
{
	if (dbg_started)
	{
		del_menu_item("Debugger/" /*EXODUS_MAIN_MENU "/" EXODUS_VDP_DEBUG_MENU "/"*/ EXODUS_VDP_PLANE_VIEWER_MENU);
	}
}

static void install_exodus_vdp_debug_menus()
{
	if (dbg_started)
	{
		if (add_menu_item("Debugger/" EXODUS_MAIN_MENU,
			EXODUS_VDP_DEBUG_MENU,
			NULL,
			SETMENU_APP | SETMENU_CTXAPP,
			NULL,
			NULL))
		{
			install_exodus_vdp_vdp_palette_menu();
		}
	}
}

static void remove_exodus_vdp_debug_menus()
{
	if (dbg_started)
	{
		remove_exodus_vdp_vdp_palette_menu();
		
		del_menu_item("Debugger/" EXODUS_MAIN_MENU "/" EXODUS_VDP_DEBUG_MENU);
	}
}

static void install_exodus_menus()
{
	if (dbg_started)
	{
		if (add_menu_item("Debugger/StepInto",
			EXODUS_MAIN_MENU,
			NULL,
			SETMENU_INS | SETMENU_CTXAPP,
			NULL,
			NULL))
		{
			install_exodus_vdp_debug_menus();
		}
	}
}

static void remove_exodus_menus()
{
	if (dbg_started)
	{
		remove_exodus_vdp_debug_menus();

		del_menu_item("Debugger/" EXODUS_MAIN_MENU);
	}
}

cli_t *reg_cli = NULL;

static bool execute_mame_cmd(const char *cmd)
{
	struct ida_local mame_cmd_t : public exec_request_t
	{
		const char *line;
		int idaapi execute(void)
		{
			return (debug_console_execute_command(*g_running_machine, line, FALSE) == CMDERR_NONE);
		}
		mame_cmd_t(const char *_line) : line(_line) {}
	};
	mame_cmd_t exec(cmd);
	return execute_sync(exec, MFF_FAST);
}

// callback: the user pressed Enter
// CLI is free to execute the line immediately or ask for more lines
// Returns: true-executed line, false-ask for more lines
static bool idaapi execute_mame_line(const char *line)
{
    return execute_mame_cmd(line);
}

static void install_remove_mame_cli(bool install)
{
	if (!reg_cli)
	{
		cli_t *cli = new cli_t();

		cli->size = sizeof(cli_t);
		cli->flags = 0;
		cli->sname = "MAME";
		cli->lname = "MAME - Debugger Console";
		cli->hint = "Execute command";
		cli->execute_line = execute_mame_line;
		cli->complete_line = NULL;
		cli->keydown = NULL;

		reg_cli = cli;
	}

	if (install)
	{
		install_command_interpreter(reg_cli);
		
	}
	else
	{
		remove_command_interpreter(reg_cli);
		reg_cli = NULL;
	}
}

static int idaapi hook_dbg(void *user_data, int notification_code, va_list va)
{
	switch (notification_code)
	{
	case dbg_notification_t::dbg_process_start:
		dbg_started = true;
		//install_exodus_menus();
		install_exodus_vdp_vdp_palette_menu();
		install_exodus_vdp_plane_viewer_menu();
		install_remove_mame_cli(true);
		break;

	case dbg_notification_t::dbg_process_exit:
		//remove_exodus_menus();

		remove_exodus_vdp_vdp_palette_menu();
		remove_exodus_vdp_plane_viewer_menu();
		install_remove_mame_cli(false);
		dbg_started = false;
		break;

	case dbg_notification_t::dbg_bpt_changed:
		int bptev_code = va_arg(va, int);
		bpt_t *bpt = va_arg(va, bpt_t *);

		if (bpt->ea & 0xFF000000)
		{
			if (bptev_code == BPTEV_ADDED)
			{
				bpt_t upd = *bpt;
				del_bpt(upd.loc);
				upd.ea &= 0xFFFFFF;
				add_bpt(upd);
			}
		}
		break;
	}
	return 0;
}

static int idaapi idp_to_dbg_reg(int idp_reg)
{
	int reg_idx = idp_reg;
	if (idp_reg >= 0 && idp_reg <= 7)
		reg_idx = R_D0 + idp_reg;
	else if (idp_reg >= 8 && idp_reg <= 39)
		reg_idx = R_A0 + (idp_reg % 8);
	else if (idp_reg == 91)
		reg_idx = R_PC;
	else if (idp_reg == 92 || idp_reg == 93)
		reg_idx = R_SR;
	else if (idp_reg == 94)
		reg_idx = R_USP;
	else
	{
		char buf[MAXSTR];
		qsnprintf(buf, MAXSTR, "reg: %d\n", idp_reg);
		warning("SEND THIS MESSAGE TO meffi@lab313.ru:\n%s\n", buf);
		return 0;
	}
	return reg_idx;
}

static int idaapi hook_ui(void *user_data, int notification_code, va_list va)
{
	switch (notification_code)
	{
	case ui_notification_t::ui_get_custom_viewer_hint:
	{
		TCustomControl *viewer = va_arg(va, TCustomControl *);
		place_t *place = va_arg(va, place_t *);
		int *important_lines = va_arg(va, int *);
		qstring &hint = *va_arg(va, qstring *);

		if (place == NULL)
			return 0;

		int x, y;
		if (get_custom_viewer_place(viewer, true, &x, &y) == NULL)
			return 0;

		char buf[MAXSTR];
		const char *line = get_custom_viewer_curline(viewer, true);
		tag_remove(line, buf, sizeof(buf));
		if (x >= (int)strlen(buf))
			return 0;

		idaplace_t &pl = *(idaplace_t *)place;
		if (ua_ana0(pl.ea) && dbg_started)
		{
			insn_t _cmd = cmd;

			int flags = calc_default_idaplace_flags();
			linearray_t ln(&flags);

			for (int i = 0; i < qnumber(_cmd.Operands); i++)
			{
				op_t op = _cmd.Operands[i];

				if (op.type != o_void)
				{
					switch (op.type)
					{
					case o_mem:
					case o_near:
					{
						idaplace_t here;
						here.ea = op.addr;
						here.lnnum = 0;

						ln.set_place(&here);

						hint.cat_sprnt((COLSTR(SCOLOR_INV"OPERAND#%d (ADDRESS: $%a)\n", SCOLOR_DREF)), op.n, op.addr);
						(*important_lines)++;

						int n = qmin(ln.get_linecnt(), 10);		   // how many lines for this address?
						(*important_lines) += n;
						for (int j = 0; j < n; ++j)
						{
							hint.cat_sprnt("%s\n", ln.down());
						}
					} break;
					case o_phrase:
					case o_reg:
					{
						regval_t reg;
						int reg_idx = idp_to_dbg_reg(op.reg);

						const char *reg_name = dbg->registers[reg_idx].name;
						if (get_reg_val(reg_name, &reg))
						{
							idaplace_t here;
							here.ea = (uint32)reg.ival;
							here.lnnum = 0;

							ln.set_place(&here);

							hint.cat_sprnt((COLSTR(SCOLOR_INV"OPERAND#%d (REGISTER: %s)\n", SCOLOR_DREF)), op.n, reg_name);
							(*important_lines)++;

							int n = qmin(ln.get_linecnt(), 10);		   // how many lines for this address?
							(*important_lines) += n;
							for (int j = 0; j < n; ++j)
							{
								hint.cat_sprnt("%s\n", ln.down());
							}
						}
					} break;
					case o_displ:
					{
						regval_t main_reg, add_reg;
						int main_reg_idx = idp_to_dbg_reg(op.reg);
						int add_reg_idx = idp_to_dbg_reg(op.specflag1 & 0xF);

						main_reg.ival = 0;
						add_reg.ival = 0;
						if (op.specflag2 & 0x10)
						{
							get_reg_val(dbg->registers[add_reg_idx].name, &add_reg);
							if (op.specflag1 & 0x10)
								add_reg.ival &= 0xFFFF;
						}

						if (main_reg_idx != R_PC)
							get_reg_val(dbg->registers[main_reg_idx].name, &main_reg);

						idaplace_t here;
						ea_t addr = (uint32)main_reg.ival + op.addr + (uint32)add_reg.ival; // TODO: displacements with PC and other regs unk_123(pc, d0.l); unk_111(d0, d2.w)
						here.ea = addr;
						here.lnnum = 0;

						ln.set_place(&here);

						hint.cat_sprnt((COLSTR(SCOLOR_INV"OPERAND#%d (DISPLACEMENT: [$%s%X($%X", SCOLOR_DREF)),
							op.n,
							((int)op.addr < 0) ? "-" : "", ((int)op.addr < 0) ? -(uint32)op.addr : op.addr,
							(uint32)main_reg.ival
							);

						if (op.specflag2 & 0x10)
							hint.cat_sprnt((COLSTR(",$%X", SCOLOR_DREF)), (uint32)add_reg.ival);

						hint.cat_sprnt((COLSTR(")])\n", SCOLOR_DREF)));

						(*important_lines)++;

						int n = qmin(ln.get_linecnt(), 10);		   // how many lines for this address?
						(*important_lines) += n;
						for (int j = 0; j < n; ++j)
						{
							hint.cat_sprnt("%s\n", ln.down());
						}
					} break;
					}
				}
			}

			return 1;
		}
	}
	default:
		return 0;
	}
}

//--------------------------------------------------------------------------
static void print_version()
{
	static const char format[] = "MESSIDA debugger plugin v%s;\nAuthor: Dr. MefistO [Lab 313] <meffi@lab313.ru>.";
	info(format, VERSION);
	msg(format, VERSION);
}

//--------------------------------------------------------------------------
// Initialize debugger plugin
static bool init_plugin(void)
{
	if (ph.id != PLFM_68K)
		return false;

	return true;
}

//--------------------------------------------------------------------------
// Initialize debugger plugin
static int idaapi init(void)
{
	if (init_plugin())
	{
		dbg = &debugger;
		plugin_inited = true;
		dbg_started = false;
		hook_to_notification_point(HT_UI, hook_ui, NULL);
		hook_to_notification_point(HT_DBG, hook_dbg, NULL);
		hook_to_notification_point(HT_IDP, hook_idp, NULL);

		print_version();
		return PLUGIN_KEEP;
	}
	return PLUGIN_SKIP;
}

//--------------------------------------------------------------------------
// Terminate debugger plugin
static void idaapi term(void)
{
	if (plugin_inited)
	{
		//term_plugin();
		unhook_from_notification_point(HT_UI, hook_ui, NULL);
		unhook_from_notification_point(HT_DBG, hook_dbg, NULL);
		unhook_from_notification_point(HT_IDP, hook_idp, NULL);
		plugin_inited = false;
		dbg_started = false;
	}
}

//--------------------------------------------------------------------------
// The plugin method - usually is not used for debugger plugins
static void idaapi run(int /*arg*/)
{
}

//--------------------------------------------------------------------------
char comment[] = "MESSIDA debugger plugin by Dr. MefistO.";

char help[] =
"MESSIDA debugger plugin by Dr. MefistO.\n"
"\n"
"This module lets you debug Genesis roms in MESS.\n";

//--------------------------------------------------------------------------
//
//      PLUGIN DESCRIPTION BLOCK
//
//--------------------------------------------------------------------------
plugin_t PLUGIN =
{
	IDP_INTERFACE_VERSION,
	PLUGIN_PROC | PLUGIN_HIDE | PLUGIN_DBG | PLUGIN_MOD, // plugin flags
	init, // initialize

	term, // terminate. this pointer may be NULL.

	run, // invoke plugin

	comment, // long comment about the plugin
	// it could appear in the status line
	// or as a hint

	help, // multiline help about the plugin

	"MESSIDA debugger plugin", // the preferred short name of the plugin

	"" // the preferred hotkey to run the plugin
};