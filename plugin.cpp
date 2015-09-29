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

#define VERSION "1.5.1"

#include <Windows.h>

#include <ida.hpp>
#include <dbg.hpp>
#include <idd.hpp>
#include <loader.hpp>

#undef interface

#include "emu.h"
#include "debugcpu.h"
#include "debugcon.h"

#include "mess_debmod.h"
#include "registers.h"

#include "resource.h"
#include "vdp_ram.h"

extern debugger_t debugger;
extern running_machine *g_running_machine;

static bool plugin_inited;
static bool dbg_started;

HWND VDPRamHWnd = NULL;
//HINSTANCE g_hinstance = NULL;
//HWND HWnd = NULL;

LRESULT CALLBACK VDPRamProc(HWND, UINT, WPARAM, LPARAM);

static HINSTANCE GetHInstance()
{
    MEMORY_BASIC_INFORMATION mbi;
    SetLastError(ERROR_SUCCESS);
    VirtualQuery(GetHInstance, &mbi, sizeof(mbi));

    return (HINSTANCE)mbi.AllocationBase;
}

static char *get_type(int type)
{
	switch (type)
	{
	case 0: return "o_void";
	case 1: return "o_reg";
	case 2: return "o_mem";
	case 3: return "o_phrase";
	case 4: return "o_displ";
	case 5: return "o_imm";
	case 6: return "o_far";
	case 7: return "o_near";
	case 8: return "o_idpspec0";
	case 9: return "o_idpspec1";
	case 10: return "o_idpspec2";
	case 11: return "o_idpspec3";
	case 12: return "o_idpspec4";
	case 13: return "o_idpspec5";
	default:
		return "bad o_type";
	}
}

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

static bool idaapi create_vdp_ram_window(void *ud)
{
    if (!VDPRamHWnd)
        VDPRamHWnd = CreateDialog(GetHInstance(), MAKEINTRESOURCE(IDD_VDPRAM), NULL, (DLGPROC)VDPRamProc);
    else
        SetForegroundWindow(VDPRamHWnd);

    return true;
}

#define SHELL_MOD_VRAM "VDP RAM"

static void remove_shell_vram_menu()
{
    if (dbg_started)
        del_menu_item("Debugger/" SHELL_MOD_VRAM);
}

//---------------------------------------------------------------------------
static void install_shell_vram_menu()
{
    if (dbg_started)
        add_menu_item("Debugger/StepInto",
        SHELL_MOD_VRAM,
        NULL,
        SETMENU_INS | SETMENU_CTXAPP,
        create_vdp_ram_window,
        NULL);
}

static int idaapi hook_dbg(void *user_data, int notification_code, va_list va)
{
	switch (notification_code)
	{
	case dbg_notification_t::dbg_process_start:
        dbg_started = true;
        install_shell_vram_menu();
		break;

	case dbg_notification_t::dbg_process_exit:
        remove_shell_vram_menu();
        dbg_started = false;
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
	else if (idp_reg == 93)
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
					//qsnprintf(buf, MAXSTR, "type: %s\n", get_type(op.type));
					//OutputDebugStringA(buf);

					qsnprintf(buf, MAXSTR, "cmd: %d, type: %s, phrase: %d, addr: %a, value: %a, sp1: %x, sp2: %x\n", _cmd.itype, get_type(op.type), op.reg, op.addr, op.value, op.specflag1, op.specflag2);
					OutputDebugStringA(buf);

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

						int n = qmin(ln.get_linecnt(), 10);           // how many lines for this address?
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

							int n = qmin(ln.get_linecnt(), 10);           // how many lines for this address?
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

						qsnprintf(buf, MAXSTR, "reg: %d, sp1: %x, sp2: %x\n", op.reg, op.specflag1, op.specflag2);
						OutputDebugStringA(buf);

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

						int n = qmin(ln.get_linecnt(), 10);           // how many lines for this address?
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