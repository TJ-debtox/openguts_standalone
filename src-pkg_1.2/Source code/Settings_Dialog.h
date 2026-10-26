/*----------------------------------------------------------------------------

	GPLv3 LICENSE INFORMATION

	This file is part of free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License (version 3) as
	published by the Free Software Foundation. This program is distributed in
	the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
	implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
	You should have received a copy of the GPLv3 along with this software. For
	detailed information, please refer to this copy of the license. Do not remove
	or change this text.

	Preamble
	The following C++ code has been developed based on the MATLAB version (the
	original GUTS prototype software) written in MATLAB by Tjalling Jager
	(http://www.debtox.info/byom.html). The project objective was to translate the
	MATLAB code into C++ code sticking as closely as possible to the original
	MATLAB code and data structures. Also the same names of variables and
	functions were used as in the MATLAB code. Therefore, parts of the present
	code might look slightly complicated and some are redundant. Also comments in
	the code are mostly taken from the MATLAB version.

----------------------------------------------------------------------------*/

//----------------------------------------------------------------------------
#ifndef Settings_DialogH
#define Settings_DialogH
//----------------------------------------------------------------------------
#include <Vcl.ExtCtrls.hpp>
#include <Vcl.Buttons.hpp>
#include <Vcl.StdCtrls.hpp>
#include <Vcl.Controls.hpp>
#include <Vcl.Forms.hpp>
#include <Vcl.Graphics.hpp>
#include <System.Classes.hpp>
#include <System.SysUtils.hpp>
#include <Winapi.Windows.hpp>
#include <System.hpp>
#include <Vcl.Dialogs.hpp>
//----------------------------------------------------------------------------
class TSettingsDlg : public TForm
{
__published:
	TPanel *Panel1;
	TPanel *Panel2;
	TButton *OKBtn;
	TPanel *Panel4;
	TLabel *Label1;
	TEdit *OutputFolderEdit;
	TSpeedButton *SpeedButton1;
	TLabel *Label2;
	TEdit *Edit1;
	TSpeedButton *SpeedButton2;
private:
public:
	virtual __fastcall TSettingsDlg(TComponent* AOwner);
};
//----------------------------------------------------------------------------
extern PACKAGE TSettingsDlg *SettingsDlg;
//----------------------------------------------------------------------------
#endif    
