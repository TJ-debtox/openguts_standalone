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

//---------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop
#include <tchar.h>
//---------------------------------------------------------------------
USEFORM("main.CPP", MainForm);
USEFORM("ProjectDescription_Dialog.cpp", ProjectDescriptionDlg);
USEFORM("ABOUT.CPP", AboutBox);
USEFORM("BatchPrediction_Dialog.cpp", BatchPredictionDlg);
USEFORM("userInfo.cpp", FormUserInfo);
//---------------------------------------------------------------------------
int WINAPI _tWinMain(HINSTANCE, HINSTANCE, LPTSTR, int)
{
	Application->Initialize();
	Application->Title = "openGUTS";
		Application->CreateForm(__classid(TMainForm), &MainForm);
		Application->CreateForm(__classid(TProjectDescriptionDlg), &ProjectDescriptionDlg);
		Application->CreateForm(__classid(TBatchPredictionDlg), &BatchPredictionDlg);
		Application->CreateForm(__classid(TAboutBox), &AboutBox);
		Application->CreateForm(__classid(TFormUserInfo), &FormUserInfo);
		Application->Run();

	return 0;
}
//---------------------------------------------------------------------
