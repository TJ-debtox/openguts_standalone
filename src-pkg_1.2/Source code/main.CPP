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

#include <vcl.h>
#pragma hdrstop

#include "main.h"
#include "About.h"
#include "ProjectDescription_Dialog.h"
#include "BatchPrediction_Dialog.h"
#include "userInfo.h"

// ---------------------------------------------------------------------
#pragma link "Word_XP_srvr"
#pragma link "Word_XP_srvr"
#pragma resource "*.dfm"
TMainForm *MainForm;
// ---------------------------------------------------------------------

string c_str(UnicodeString ustr)
{
	return AnsiString(ustr).c_str();
}

// ---------------------------------------------------------------------

int CountProcesses(AnsiString executable_file)
// counts the number of running processes with a given exe-name (file name)
{
	PROCESSENTRY32 *ProcEntry = new PROCESSENTRY32();
	ProcEntry->dwSize = sizeof(PROCESSENTRY32);
	HANDLE hProcess;
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	bool Result;
	Result=Process32First(hSnapshot, ProcEntry);
	int n=0;
	while (Result)
	{
		if (ExtractFileName(StrPas(ProcEntry->szExeFile))==executable_file)
			n++;
		Result = Process32Next(hSnapshot, ProcEntry);
	}
	CloseHandle(hSnapshot);
	delete(ProcEntry);
	return n;
}
// ---------------------------------------------------------------------

__fastcall TMainForm::TMainForm(TComponent *AOwner) : TForm(AOwner)
{
	 LPxBatch_out.assign(0,"");

	 MaximumSurvivalTime = 0;
}
// ---------------------------------------------------------------------

void __fastcall TMainForm::acFileNewExecute(TObject *Sender)
{
	if (MessageDlg("Start new project?", mtConfirmation, TMsgDlgButtons() << mbOK << mbCancel, 0) == mrOk)
	{
		Reset(true);    // reset model parameters
		ProjectFileName = "";
		ProjectFileSaveDialog->FileName = "";
		Caption = SOFTWARE_NAME + " " + SOFTWARE_VERSION;
	}

	return;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::acFileOpenExecute(TObject *Sender)
{
	if (ProjectFileOpenDialog->Execute())
	{
		MainPageControl->Enabled = false;
		Reset(true);
		CurrentlyLoadingProjectFile = true;
		FormUserInfo->MemoPad->Lines->Clear();
		FormUserInfo->MemoPad->Lines->Add("Loading project file: ");
		FormUserInfo->MemoPad->Lines->Add(AnsiString(ProjectFileOpenDialog->FileName));
		FormUserInfo->Show();
		if (!OpenProjectFromFile(AnsiString(ProjectFileOpenDialog->FileName).c_str()))
		{
			FormUserInfo->MemoPad->Lines->Clear();
			FormUserInfo->Hide();
			Reset(true);
			AnsiString filename = AnsiString(ProjectFileOpenDialog->FileName);
			AnsiString ErrorMsg = "Opening the project file failed." +
			AnsiString("Please check the format of the provided project file.\n");
			if (ProjectFileVersion.compare("1.4") < 0) {
				ErrorMsg +=
				AnsiString("\n\nNote:\n") +
				AnsiString("Loading a project file from an older openGUTS version.") +
				AnsiString(".\n") +
				AnsiString("In openGUTS versions < 1.1,") +
				AnsiString(" the project file could be corrupted while performing projections.") +
				AnsiString("\n") +
				AnsiString("Please load a project file that was stored prior to conducting projections") +
				AnsiString(" or rerun calibration and validation for this project with the current openGUTS version.");
			}
			ShowMessage(ErrorMsg);
			return;
		}
        CurrentlyLoadingProjectFile = false;
		Screen->Cursor = crHourGlass;
		ProjectFileName = ProjectFileOpenDialog->FileName;
		Caption = SOFTWARE_NAME + " " + SOFTWARE_VERSION + "  - " + ProjectFileName;

		// clear eventually remained structures
		CalibrationProfileLikelihoodsSD.resize(0);
		CalibrationProfileLikelihoodsIT.resize(0);

		DisplayInformationAboutLoadedProjectFile();

		CalibrationSDMemo->Lines->Add("Best estimates and 95% CIs on single parameters");
		Disp_PMat(PMatSD, gutsSD, PMatPrintSD, CalibrationSDMemo, true);
		CalibrationITMemo->Lines->Add("Best estimates and 95% CIs on single parameters");
		Disp_PMat(PMatIT, gutsIT, PMatPrintIT, CalibrationITMemo, true);

		if (CalibrationPerformed)
			FormUserInfo->MemoPad->Lines->Add("");
			FormUserInfo->MemoPad->Lines->Add("Retrieving calibration plots.");
            FormUserInfo->MemoPad->Lines->Add("Please wait. This might take a moment ... ");
			ReconstructCalibrationPlots();

		// deny validation and predictions
		SDValidationPerformed = false;
		ITValidationPerformed = false;
		SDPredictionsPerformed = false;
		ITPredictionsPerformed = false;

		Screen->Cursor = crDefault;
		MainPageControl->Enabled = true;

		FormUserInfo->MemoPad->Lines->Add("Project completely loaded.");
		Sleep(2000);
		FormUserInfo->MemoPad->Lines->Clear();
        FormUserInfo->Hide();
	}

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::ReconstructCalibrationPlots()
{
	UpdateParameterValuesInGUI();

	// Paint the calibration results
	PaintParameterSpacePlot();
	double hb_fix = -1.0;	// identifyer (don't fix background mortality in sample)

	TMemo *memo;
	TImage *dummy_img;
	AnsiString error = "";

	// for SD
	// paint GUTS graphs
	PaintGUTSGraphs(CalibData_All, NumberOfCalibrationDataSets, hb_fix, CalibSDSurvivalScrollBox, dummy_img, CALIBRATION, gutsSD, error);
	if (error != "")
	{
		ShowMessage(error);
		Reset(false);
		Screen->Cursor = crDefault;
		return;
	}
	// write GOODNESS OF FIT to memo
	DisplayGoodnessOfFit(CalibData_All, CalibrationSDMemo, gutsSD, CALIBRATION);
	// calc and paint LCx
	Calc_LCx(LCxSD, LCxSD_low, LCxSD_high, LCX_T_END, gutsSD, error);
	if (error != "")
	{
		ShowMessage(error);
		return;
	}
	PlotLCx(CalibrationSDOutputLCxImage, gutsSD, LCX_T_END);

	// for IT
	// paint GUTS graphs
	PaintGUTSGraphs(CalibData_All, NumberOfCalibrationDataSets, hb_fix, CalibITSurvivalScrollBox, dummy_img, CALIBRATION, gutsIT, error);
	if (error != "")
	{
		ShowMessage(error);
		Reset(false);
		Screen->Cursor = crDefault;
		return;
	}
	// write GOODNESS OF FIT to memo
	DisplayGoodnessOfFit(CalibData_All, CalibrationITMemo, gutsIT, CALIBRATION);
    // calc and paint LCx
	Calc_LCx(LCxIT, LCxIT_low, LCxIT_high, LCX_T_END, gutsIT, error);
	if (error != "")
	{
		ShowMessage(error);
		return;
	}
	PlotLCx(CalibrationITOutputLCxImage, gutsIT, LCX_T_END);


	CalibrationResultsPlotted = true;

	CalibrationStatusLabel->Font->Color = clBlue;
	CalibrationStatusLabel->Caption = ("Calibration data loaded from file '" + BaseFileName(c_str(ProjectFileName)) + "'").c_str();
	CalibrationSDOutputPageControl->Visible = true;
	CalibrationITOutputPageControl->Visible = true;

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::DisplayInformationAboutLoadedProjectFile()
{
	// information for calibration
	CalibrationStatusLabel->Font->Color = clBlue;
	CalibrationStatusLabel->Caption = ("Calibration data loaded from file '" + BaseFileName(c_str(ProjectFileName)) + "'").c_str();

	CalibrationSDMemo->Clear();
	CalibrationSDMemo->Lines->Add(("Calibration data loaded from file '" + BaseFileName(c_str(ProjectFileName)) + "'").c_str());
	CalibrationITMemo->Clear();
	CalibrationITMemo->Lines->Add(("Calibration data loaded from file '" + BaseFileName(c_str(ProjectFileName)) + "'").c_str());
	if (CalibrationPerformed)
	{
		CalibrationSDMemo->Lines->Add("Calibration successfully performed");
		CalibrationITMemo->Lines->Add("Calibration successfully performed");
	}
	else
	{
		CalibrationSDMemo->Lines->Add("Calibration not performed");
		CalibrationITMemo->Lines->Add("Calibration not performed");
	}

	// information for validation
	ValidationSDOutputMemo->Clear();
	ValidationSDOutputMemo->Lines->Add(("Validation data loaded from project file '" + BaseFileName(c_str(ProjectFileName)) + "'").c_str());
	ValidationSDOutputMemo->Lines->Add("Please validate the data again to generate the associated graphs");
	ValidationITOutputMemo->Clear();
	ValidationITOutputMemo->Lines->Add(("Validation data loaded from project file '" + BaseFileName(c_str(ProjectFileName)) + "'").c_str());
	ValidationITOutputMemo->Lines->Add("Please validate the data again to generate the associated graphs");

	// information for predictions
	PredictionsSDOutputMemo->Clear();
	PredictionsSDOutputMemo->Lines->Add(("Predictions data loaded from project file '" + BaseFileName(c_str(ProjectFileName)) + "'").c_str());
	PredictionsSDOutputMemo->Lines->Add("Please run the predictions again to generate the associated graphs");
	PredictionsITOutputMemo->Clear();
	PredictionsITOutputMemo->Lines->Add(("Predictions data loaded from project file '" + BaseFileName(c_str(ProjectFileName)) + "'").c_str());
	PredictionsITOutputMemo->Lines->Add("Please run the predictions again to generate the associated graphs");

	return;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::acFileSaveExecute(TObject *Sender)
{
	if (ProjectFileName == "")
	{
		if (!ProjectFileSaveDialog->Execute())
			return;
		ProjectFileName = ProjectFileSaveDialog->FileName;
	}
	if (SaveProjectToFile(AnsiString(ProjectFileName).c_str()))
		Caption = SOFTWARE_NAME + " " + SOFTWARE_VERSION + "  - " + ProjectFileName;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::acFileExitExecute(TObject *Sender)
{
	Close();
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::HelpAbout1Execute(TObject *Sender)
{
	AboutBox->ShowModal();
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::FormCreate(TObject *Sender)
{
	OldDecimalSeparator = FormatSettings.DecimalSeparator;
	FormatSettings.DecimalSeparator = '.';

	OldMainFormWidth = MainForm->Width;
	OldMainFormHeight = MainForm->Height;

	Caption = SOFTWARE_NAME + " " + SOFTWARE_VERSION;
	ProjectFileOpenDialog->DefaultExt = PROJECT_FILE_EXT;
	ProjectFileOpenDialog->Filter = SOFTWARE_NAME + " project file (*." + PROJECT_FILE_EXT + ")|*." + PROJECT_FILE_EXT +
		"|All Files (*.*)|*.*";
	ProjectFileOpenDialog->FilterIndex = 0;
	ProjectFileSaveDialog->DefaultExt = ProjectFileOpenDialog->DefaultExt;
	ProjectFileSaveDialog->Filter = ProjectFileOpenDialog->Filter;
	ProjectFileSaveDialog->FilterIndex = 0;

	InputFileOpenDialog->DefaultExt = INPUT_DATA_FILE_EXT;
	InputFileOpenDialog->Filter = SOFTWARE_NAME + " data file (*." + INPUT_DATA_FILE_EXT + ")|*." + INPUT_DATA_FILE_EXT +
		"|All Files (*.*)|*.*";
	InputFileOpenDialog->FilterIndex = 0;
	InputFileSaveDialog->DefaultExt = InputFileOpenDialog->DefaultExt;
	InputFileSaveDialog->Filter = InputFileOpenDialog->Filter;
	InputFileSaveDialog->FilterIndex = 0;

	Randomize();

	// Load data (text and images)

	// get path to info boxes
	wchar_t executable_path[MAX_PATH];
	GetModuleFileName(NULL, executable_path, MAX_PATH);
	AnsiString full_exe_path = AnsiString(executable_path);
	string exe_path_without_filename = PathWithoutFile(full_exe_path.c_str());

	// ************************************
	// ********** INPUT DATA TAB **********
	// ************************************
	WebBrowser1->Navigate(AnsiString(exe_path_without_filename.c_str()) + "info\\InfoBox1.html");
	CalibrationDataSetTabs[0] = CalibDataTab1;
	CalibrationDataSetTabs[1] = CalibDataTab2;
	CalibrationDataSetTabs[2] = CalibDataTab3;
	CalibrationDataSetTabs[3] = CalibDataTab4;
	CalibrationDataSetTabs[4] = CalibDataTab5;
	CalibConcStringGrids[0] = CalibConcStringGrid1;
	CalibConcStringGrids[1] = CalibConcStringGrid2;
	CalibConcStringGrids[2] = CalibConcStringGrid3;
	CalibConcStringGrids[3] = CalibConcStringGrid4;
	CalibConcStringGrids[4] = CalibConcStringGrid5;
	CalibSurvStringGrids[0] = CalibSurvStringGrid1;
	CalibSurvStringGrids[1] = CalibSurvStringGrid2;
	CalibSurvStringGrids[2] = CalibSurvStringGrid3;
	CalibSurvStringGrids[3] = CalibSurvStringGrid4;
	CalibSurvStringGrids[4] = CalibSurvStringGrid5;
	CalibControlCombos[0] = CalibControlCombo1;
	CalibControlCombos[1] = CalibControlCombo2;
	CalibControlCombos[2] = CalibControlCombo3;
	CalibControlCombos[3] = CalibControlCombo4;
	CalibControlCombos[4] = CalibControlCombo5;
	CalibDataDescriptionRichEdits[0] = InputDataDescriptionRichEdit1;
	CalibDataDescriptionRichEdits[1] = InputDataDescriptionRichEdit2;
	CalibDataDescriptionRichEdits[2] = InputDataDescriptionRichEdit3;
	CalibDataDescriptionRichEdits[3] = InputDataDescriptionRichEdit4;
	CalibDataDescriptionRichEdits[4] = InputDataDescriptionRichEdit5;
	NumberOfCalibrationDataSets = 1;
	InputDataCalibSetNoSpin->Value = NumberOfCalibrationDataSets;

	// hide inspect tab
	InputInspectDataTab->Visible = false;

	// *************************************
	// ********** CALIBRATION TAB **********
	// *************************************
	ExpertSettingsPageControl->Visible = ShowExpertSettingsCheckBox->Checked;
	WebBrowser2->Navigate(AnsiString(exe_path_without_filename.c_str()) + "info\\InfoBox2.html");

	// ************************************
	// ********** VALIDATION TAB **********
	// ************************************
	WebBrowser3->Navigate(AnsiString(exe_path_without_filename.c_str()) + "info\\InfoBox3.html");
	ValidationSDOutputSurvivalImage->OnMouseDown = ImageMouseDown;
	ValidationITOutputSurvivalImage->OnMouseDown = ImageMouseDown;


	// *************************************
	// ********** PREDICTIONS TAB **********
	// *************************************
	//
	PredictionsConcStringGrid->Cells[0][0] = "Time [d]";
	PredictionsConcStringGrid->Cells[1][0] = "Concentration";
	WebBrowser4->Navigate(AnsiString(exe_path_without_filename.c_str()) + "info\\InfoBox4.html");

	PredictionsSDOutputTab->Visible = false;
	PredictionsITOutputTab->Visible = false;
	PredictionsSDOutputTab->TabVisible = false;
	PredictionsITOutputTab->TabVisible = false;
	// select exposure profile, not batch processing as default:
	// enable all exposure profile elements
	PredDataOpenSpeed->Enabled = true;
	PredDataSaveSpeed->Enabled = true;
	PredictionsConcStringGrid->Enabled = true;
	// set IT checked as default
	PredictionsGutsITCheckBox->Checked = true;

	// batch predictions
	BatchPredictionSurvivalImageSD = new TImage(this);
	BatchPredictionSurvivalImageIT = new TImage(this);
	BatchPredictionMultiFacImageSD = new TImage(this);
	BatchPredictionMultiFacImageIT = new TImage(this);

	// set active tabs
	MainPageControl->ActivePage = InputTab;
	InputDataPageControl2->ActivePage = InputInfoTab;
	CalibrationPageControl->ActivePage = CalibrationInfoTab;
	ValidationPageControl->ActivePage = ValidationInfoTab;
	PredictionsPageControl->ActivePage = PredictionsInfoTab;

	// replace parameter names
	// for SD
	GUTSSDParamLabels1[0] = SDParamKDLabel1;
	GUTSSDParamLabels1[1] = SDParamMWLabel1;
	GUTSSDParamLabels1[2] = SDParamHBLabel1;
	GUTSSDParamLabels1[3] = SDParamBWLabel1;
	GUTSSDParamLabels1[4] = SDParamFSLabel1;
	GUTSSDParamLabels2[0] = SDParamKDLabel2;
	GUTSSDParamLabels2[1] = SDParamMWLabel2;
	GUTSSDParamLabels2[2] = SDParamHBLabel2;
	GUTSSDParamLabels2[3] = SDParamBWLabel2;
	GUTSSDParamLabels2[4] = SDParamFSLabel2;

	GUTSSDParamValueEdits[0] = SDParamKDValueEdit;
	GUTSSDParamValueEdits[1] = SDParamMWValueEdit;
	GUTSSDParamValueEdits[2] = SDParamHBValueEdit;
	GUTSSDParamValueEdits[3] = SDParamBWValueEdit;
	GUTSSDParamValueEdits[4] = SDParamFSValueEdit;

	GUTSSDParamMinEdits[0] = SDParamKDMinEdit;
	GUTSSDParamMinEdits[1] = SDParamMWMinEdit;
	GUTSSDParamMinEdits[2] = SDParamHBMinEdit;
	GUTSSDParamMinEdits[3] = SDParamBWMinEdit;
	GUTSSDParamMinEdits[4] = SDParamFSMinEdit;

	GUTSSDParamMaxEdits[0] = SDParamKDMaxEdit;
	GUTSSDParamMaxEdits[1] = SDParamMWMaxEdit;
	GUTSSDParamMaxEdits[2] = SDParamHBMaxEdit;
	GUTSSDParamMaxEdits[3] = SDParamBWMaxEdit;
	GUTSSDParamMaxEdits[4] = SDParamFSMaxEdit;

	GUTSSDParamLogChecks[0] = SDParamKDLogCheck;
	GUTSSDParamLogChecks[1] = SDParamMWLogCheck;
	GUTSSDParamLogChecks[2] = SDParamHBLogCheck;
	GUTSSDParamLogChecks[3] = SDParamBWLogCheck;
	GUTSSDParamLogChecks[4] = SDParamFSLogCheck;

	GUTSSDParamFitChecks[0] = SDParamKDFitCheck;
	GUTSSDParamFitChecks[1] = SDParamMWFitCheck;
	GUTSSDParamFitChecks[2] = SDParamHBFitCheck;
	GUTSSDParamFitChecks[3] = SDParamBWFitCheck;
	GUTSSDParamFitChecks[4] = SDParamFSFitCheck;

	// for IT
	GUTSITParamLabels1[0] = ITParamKDLabel1;
	GUTSITParamLabels1[1] = ITParamMWLabel1;
	GUTSITParamLabels1[2] = ITParamHBLabel1;
	GUTSITParamLabels1[3] = ITParamBWLabel1;
	GUTSITParamLabels1[4] = ITParamFSLabel1;
	GUTSITParamLabels2[0] = ITParamKDLabel2;
	GUTSITParamLabels2[1] = ITParamMWLabel2;
	GUTSITParamLabels2[2] = ITParamHBLabel2;
	GUTSITParamLabels2[3] = ITParamBWLabel2;
	GUTSITParamLabels2[4] = ITParamFSLabel2;

	GUTSITParamValueEdits[0] = ITParamKDValueEdit;
	GUTSITParamValueEdits[1] = ITParamMWValueEdit;
	GUTSITParamValueEdits[2] = ITParamHBValueEdit;
	GUTSITParamValueEdits[3] = ITParamBWValueEdit;
	GUTSITParamValueEdits[4] = ITParamFSValueEdit;

	GUTSITParamMinEdits[0] = ITParamKDMinEdit;
	GUTSITParamMinEdits[1] = ITParamMWMinEdit;
	GUTSITParamMinEdits[2] = ITParamHBMinEdit;
	GUTSITParamMinEdits[3] = ITParamBWMinEdit;
	GUTSITParamMinEdits[4] = ITParamFSMinEdit;

	GUTSITParamMaxEdits[0] = ITParamKDMaxEdit;
	GUTSITParamMaxEdits[1] = ITParamMWMaxEdit;
	GUTSITParamMaxEdits[2] = ITParamHBMaxEdit;
	GUTSITParamMaxEdits[3] = ITParamBWMaxEdit;
	GUTSITParamMaxEdits[4] = ITParamFSMaxEdit;

	GUTSITParamLogChecks[0] = ITParamKDLogCheck;
	GUTSITParamLogChecks[1] = ITParamMWLogCheck;
	GUTSITParamLogChecks[2] = ITParamHBLogCheck;
	GUTSITParamLogChecks[3] = ITParamBWLogCheck;
	GUTSITParamLogChecks[4] = ITParamFSLogCheck;

	GUTSITParamFitChecks[0] = ITParamKDFitCheck;
	GUTSITParamFitChecks[1] = ITParamMWFitCheck;
	GUTSITParamFitChecks[2] = ITParamHBFitCheck;
	GUTSITParamFitChecks[3] = ITParamBWFitCheck;
	GUTSITParamFitChecks[4] = ITParamFSFitCheck;

	// resize grid rows and cols
	for (int i = 0; i < MAX_CALIBRATION_DATA_SETS_NO; i++)
	{
		CalibConcStringGrids[i]->RowCount = 100;
		CalibSurvStringGrids[i]->RowCount = 100;
		CalibConcStringGrids[i]->ColCount = 100;
		CalibSurvStringGrids[i]->ColCount = 100;
	}

	ValidationConcStringGrid->RowCount = 100;
	ValidationSurvStringGrid->RowCount = 100;
	ValidationConcStringGrid->ColCount = 100;
	ValidationSurvStringGrid->ColCount = 100;

	PredictionsConcStringGrid->RowCount = 100;
	PredictionsConcStringGrid->ColCount = 2; // only allow one treatment

	Reset(true);

	CurrentlyLoadingProjectFile = false;

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::SetDefaultHeader(TStringGrid *grid, int data_type)
{
	if (data_type == gutsCONC_DATA)
		grid->Cells[0][0] = "Conc. time [d]";
	else
		grid->Cells[0][0] = "Surv. time [d]";

	grid->Cells[1][0] = "Control";
	for (int i=0; i<3; i++)
		grid->Cells[i+2][0] = L"Group" + IntToStr(i+1);

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::Reset(bool reset_calibration_input)
{
	// Set default variables
	CalibrationDataChecked = false;
	ValidationDataChecked = false;
	PredictionsDataChecked = false;
	CalibrationPerformed = false;
	CalibrationResultsPlotted = false;
	SDValidationPerformed = false;
	ITValidationPerformed = false;
	ValidationResultsPlotted = false;
	SDPredictionsPerformed = false;
	ITPredictionsPerformed = false;
	PrefitHBToControl = FitHBToControlCheckBox->Checked;
	ExpertSettingsOn = ShowExpertSettingsCheckBox->Checked;
	SDUseCalculatedParameterRangesForCalib = true;
	ITUseCalculatedParameterRangesForCalib = true;
	SDUseExistingParameterSample = false;
	ITUseExistingParameterSample = false;
	FitHBToValidationControl = ValidationBGMFitCheckBox->Checked;
	DoSDBatchPredictions = false;
	DoITBatchPredictions = false;
	AbortCalibration = false;
	AbortPredictions = false;
	ValidationDataPresent = false;
	PredictionsDataPresent = false;
	ParameterRangesManipulated = false;

	// resize data objects
	PMatSD.Value.resize(gutsPARAMETER_NO);
	PMatSD.Fit.resize(gutsPARAMETER_NO);
	PMatSD.Min.resize(gutsPARAMETER_NO);
	PMatSD.Max.resize(gutsPARAMETER_NO);
	PMatSD.Scale.resize(gutsPARAMETER_NO);
	PMatIT.Value.resize(gutsPARAMETER_NO);
	PMatIT.Fit.resize(gutsPARAMETER_NO);
	PMatIT.Min.resize(gutsPARAMETER_NO);
	PMatIT.Max.resize(gutsPARAMETER_NO);
	PMatIT.Scale.resize(gutsPARAMETER_NO);
	for (int p = 0; p < gutsPARAMETER_NO; p++)
	{
		PMatSD.Value[p] = 0;
		PMatSD.Fit[p] = 0;
		PMatSD.Min[p] = 0;
		PMatSD.Max[p] = 0;
		PMatSD.Scale[p] = 0;
		PMatIT.Value[p] = 0;
		PMatIT.Fit[p] = 0;
		PMatIT.Min[p] = 0;
		PMatIT.Max[p] = 0;
		PMatIT.Scale[p] = 0;
	}
	CalibrationSDSurvivalImages.resize(MAX_CALIBRATION_DATA_SETS_NO);
	CalibrationITSurvivalImages.resize(MAX_CALIBRATION_DATA_SETS_NO);
	CalibrationDataFileNames.resize(MAX_CALIBRATION_DATA_SETS_NO);

	// CalibrationData.resize(0);
	ValidationData.resize(1);
	TestValidationData.resize(1);
	PredictionsData.resize(1);
	TestPredictionsData.resize(1);
	ValidationDataFileName = "";
	PredictionsDataFileName = "";

	// dummy survival data as proposed by Tjalling Jager
	CreateDummySurvivalDataForExposureScenario(PredictionsData);
	CreateDummySurvivalDataForExposureScenario(TestPredictionsData);

	ResetGUIElements(reset_calibration_input);
	if (reset_calibration_input)
	{
		ProjectDescription = "";
		for (int i=0; i<CalibrationDataFileNames.size(); i++)
			CalibrationDataFileNames[i] = "";
	}

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::ResetGUIElements(bool reset_calibration_input)
{
	if (reset_calibration_input)
	{
		// reset project description
		if (ProjectDescriptionDlg != NULL)
			ProjectDescriptionDlg->ProjectDescriptionRichEdit->Clear();

		// reset input data tab GUI elements
		InputDataCalibSetNoSpin->Value = 1;
		for (int i = 0; i < MAX_CALIBRATION_DATA_SETS_NO; i++)
		{
			ClearGrid(CalibConcStringGrids[i]);
			ClearGrid(CalibSurvStringGrids[i]);
			CalibDataDescriptionRichEdits[i]->Clear();
			CalibControlCombos[i]->Clear();
			SetDefaultHeader(CalibConcStringGrids[i], gutsCONC_DATA);
			SetDefaultHeader(CalibSurvStringGrids[i], gutsSURV_DATA);
		}
		ConcUnitEdit->Clear();
	}

	CalibInspectDataScrollBox->Visible = false;

	// reset calibration tab GUI elements
	for (int p=0; p<gutsPARAMETER_NO; p++)
	{
		GUTSSDParamValueEdits[p]->Text = "";
		GUTSSDParamMinEdits[p]->Text = "";
		GUTSSDParamMaxEdits[p]->Text = "";
		GUTSITParamValueEdits[p]->Text = "";
		GUTSITParamMinEdits[p]->Text = "";
		GUTSITParamMaxEdits[p]->Text = "";
	}
	GUTSSDParamValueEdits[gutsHB]->Enabled = !PrefitHBToControl;
	GUTSITParamValueEdits[gutsHB]->Enabled = !PrefitHBToControl;
	CalibrationSDMemo->Clear();
	CalibrationITMemo->Clear();
	CalibrationSDOutputPageControl->Visible = false;
	CalibrationITOutputPageControl->Visible = false;
	CalibrationStatusLabel->Font->Color = clBlue;
	CalibrationStatusLabel->Caption = "No calibration performed.";
	LCxDaysSDCombo->Clear();
	LCxDaysITCombo->Clear();
	for (int t=0; t<predT_LCX_NO; t++)
	{
		LCxDaysSDCombo->Items->Add(AnsiString(predT_LCX[t]));
		LCxDaysITCombo->Items->Add(AnsiString(predT_LCX[t]));
	}
	LCxDaysSDCombo->ItemIndex = 5;
	LCxDaysITCombo->ItemIndex = 5;

	// reset validation GUI elements
	ClearGrid(ValidationConcStringGrid);
	ClearGrid(ValidationSurvStringGrid);
	ValidationDataDescriptionRichEdit->Clear();
	SetDefaultHeader(ValidationConcStringGrid, gutsCONC_DATA);
	SetDefaultHeader(ValidationSurvStringGrid, gutsSURV_DATA);
	ValidationSDOutputMemo->Clear();
	ValidationITOutputMemo->Clear();
	ValidInspectDataScrollBox->Visible = false;
	ValidationSDOutputPageControl->Visible = false;
	ValidationITOutputPageControl->Visible = false;


	// reset predictions GUI elements
	ClearGrid(PredictionsConcStringGrid);
	PredictionsSDOutputMemo->Clear();
	PredictionsITOutputMemo->Clear();
	PredInspectDataScrollBox->Visible = false;
	PredictionsSDOutputPageControl->Visible = false;
	PredictionsITOutputPageControl->Visible = false;


	// set active tabs
	MainPageControl->ActivePage = InputTab;
	InputDataPageControl2->ActivePage = InputInfoTab;
	CalibrationPageControl->ActivePage = CalibrationInfoTab;
	ValidationPageControl->ActivePage = ValidationInfoTab;
	PredictionsPageControl->ActivePage = PredictionsInfoTab;

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::ClearValidationResults()
{
	// reset validation bools
	ValidationDataChecked = false;
	SDValidationPerformed = false;
	ITValidationPerformed = false;
	ValidationResultsPlotted = false;

	// reset GUI elements
	ValidationSDOutputMemo->Clear();
	ValidationITOutputMemo->Clear();
	ValidInspectDataScrollBox->Visible = false;
	ValidationSDOutputPageControl->Visible = false;
	ValidationITOutputPageControl->Visible = false;
	ValidationPageControl->ActivePage = ValidationInfoTab;

	// CalibrationData.resize(0);
	ValidationData.resize(1);
	TestValidationData.resize(1);

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::ClearPredictionsResults()
{
	// reset predictions bools
	PredictionsDataChecked = false;
	SDPredictionsPerformed = false;
	ITPredictionsPerformed = false;
	DoSDBatchPredictions = false;
	DoITBatchPredictions = false;
	AbortPredictions = false;

	// reset GUI elements
	PredictionsSDOutputMemo->Clear();
	PredictionsITOutputMemo->Clear();
	PredInspectDataScrollBox->Visible = false;
	PredictionsSDOutputPageControl->Visible = false;
	PredictionsITOutputPageControl->Visible = false;
    PredictionsPageControl->ActivePage = PredictionsInfoTab;

	// CalibrationData.resize(0);
	PredictionsData.resize(1);
	TestPredictionsData.resize(1);

	// dummy survival data as proposed by Tjalling Jager
	CreateDummySurvivalDataForExposureScenario(PredictionsData);
	CreateDummySurvivalDataForExposureScenario(TestPredictionsData);

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::CreateDummySurvivalDataForExposureScenario(vector<TDataSet> &data)
{
	// dummy survival data as proposed by Tjalling Jager
	data[0].Survival.GroupNames.resize(1);
	data[0].Survival.Times.resize(2);
	data[0].Survival.Times[0] = 0;
	data[0].Survival.Times[1] = 1;
	// will be adjusted to the concentration time later
	data[0].Survival.Values.resize(1);
	data[0].Survival.Values[0].resize(2);
	data[0].Survival.Values[0][0] = 10;
	data[0].Survival.Values[0][1] = 10;
	// data[0].Concentration has to be filled at runtime with according data

}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::ShowExpertSettingsCheckBoxClick(TObject *Sender)
{
	//ExpertSettingsOn = ShowExpertSettingsCheckBox->Checked;
	ExpertSettingsPageControl->Visible = ShowExpertSettingsCheckBox->Checked;

	if (ShowExpertSettingsCheckBox->Checked == false)
		ResetCalibrationSettingInGUI();

	return;
}
// ---------------------------------------------------------------------------

bool TMainForm::LoadParamDataFromGUI(TParameterMatrix &pmat, int sdit)
	// load data from the GUI and store it in PMatSD and PMatIT
{
    Application->ProcessMessages();

	pmat.Value.resize(gutsPARAMETER_NO);
	pmat.Fit.resize(gutsPARAMETER_NO);
	pmat.Min.resize(gutsPARAMETER_NO);
	pmat.Max.resize(gutsPARAMETER_NO);
	pmat.Scale.resize(gutsPARAMETER_NO);
	if (sdit == gutsSD)
	{
		// first check for correct data types
		int fit_count = 0;
		for (int p = 0; p < gutsPARAMETER_NO; p++)
		{
			if (!StringIsDouble(AnsiString(GUTSSDParamValueEdits[p]->Text).c_str()) ||
					GUTSSDParamValueEdits[p]->Text == "" ||
					!StringIsDouble(AnsiString(GUTSSDParamMinEdits[p]->Text).c_str()) ||
					GUTSSDParamMinEdits[p]->Text == "" ||
					!StringIsDouble(AnsiString(GUTSSDParamMaxEdits[p]->Text).c_str()) ||
					GUTSSDParamMaxEdits[p]->Text == "")
			{
				ShowMessage("Only enter valid float numbers as ranges and value for parameter " + AnsiString(gutsPARAMETERS[p].c_str()) + " (SD)!");
				return false;
			}

			if (GUTSSDParamValueEdits[p]->Text.ToDouble() < GUTSSDParamMinEdits[p]->Text.ToDouble() ||
					GUTSSDParamValueEdits[p]->Text.ToDouble() > GUTSSDParamMaxEdits[p]->Text.ToDouble() ||
					GUTSSDParamMinEdits[p]->Text.ToDouble() > GUTSSDParamMaxEdits[p]->Text.ToDouble())
			{
				ShowMessage("Ensure to follow the restriction MIN <= VALUE <= MAX for parameter " + AnsiString(gutsPARAMETERS[p].c_str()) + " (SD)!");
				return false;
			}

			// assign values
			pmat.Value[p] = GUTSSDParamValueEdits[p]->Text.ToDouble();
			pmat.Min[p] = GUTSSDParamMinEdits[p]->Text.ToDouble();
			pmat.Max[p] = GUTSSDParamMaxEdits[p]->Text.ToDouble();
			if (GUTSSDParamFitChecks[p]->Checked)
			{
				pmat.Fit[p] = gutsFIT_PARAM;
				fit_count++;
			}
			else
				pmat.Fit[p] = gutsNOT_FIT_PARAM;
			if (GUTSSDParamLogChecks[p]->Checked)
				pmat.Scale[p] = gutsLOG_SCALE_PARAM;
			else
				pmat.Scale[p] = gutsNORM_SCALE_PARAM;
		}
		if (fit_count < 2)
		{
			ShowMessage("Please fit at least two parameters (SD)!");
			return false;
		}
	}
	else // gutsIT
	{
		int fit_count = 0;
		for (int p = 0; p < gutsPARAMETER_NO; p++)
		{
			if (p == gutsBW)
			{
				pmat.Value[p] = -1;
				pmat.Min[p] = -1;
				pmat.Max[p] = -1;
				pmat.Fit[p] = gutsNOT_FIT_PARAM;
				pmat.Scale[p] = gutsNORM_SCALE_PARAM;
				continue;
			}

			if (!StringIsDouble(AnsiString(GUTSITParamValueEdits[p]->Text).c_str()) ||
					GUTSITParamValueEdits[p]->Text == "" ||
					!StringIsDouble(AnsiString(GUTSITParamMinEdits[p]->Text).c_str()) ||
					GUTSITParamMinEdits[p]->Text == "" ||
					!StringIsDouble(AnsiString(GUTSITParamMaxEdits[p]->Text).c_str()) ||
					GUTSITParamMaxEdits[p]->Text == "")
			{
				ShowMessage("Only enter valid float numbers as ranges and value for parameter " + AnsiString(gutsPARAMETERS[p].c_str()) + " (IT)!");
				return false;
			}

			if (GUTSITParamValueEdits[p]->Text.ToDouble() < GUTSITParamMinEdits[p]->Text.ToDouble() ||
					GUTSITParamValueEdits[p]->Text.ToDouble() > GUTSITParamMaxEdits[p]->Text.ToDouble() ||
					GUTSITParamMinEdits[p]->Text.ToDouble() > GUTSITParamMaxEdits[p]->Text.ToDouble())
			{
				ShowMessage("Ensure to follow the restriction MIN <= VALUE <= MAX for parameter " + AnsiString(gutsPARAMETERS[p].c_str()) + " (IT)!");
				return false;
			}

			// assign values
			pmat.Value[p] = GUTSITParamValueEdits[p]->Text.ToDouble();
			pmat.Min[p] = GUTSITParamMinEdits[p]->Text.ToDouble();
			pmat.Max[p] = GUTSITParamMaxEdits[p]->Text.ToDouble();
			if (GUTSITParamFitChecks[p]->Checked)
			{
				pmat.Fit[p] = gutsFIT_PARAM;
				fit_count++;
			}
			else
				pmat.Fit[p] = gutsNOT_FIT_PARAM;
			if (GUTSITParamLogChecks[p]->Checked)
				pmat.Scale[p] = gutsLOG_SCALE_PARAM;
			else
				pmat.Scale[p] = gutsNORM_SCALE_PARAM;
		}
        if (fit_count < 2)
		{
			ShowMessage("Please fit at least two parameters (IT)!");
			return false;
		}
	}

	return true;
}
// ---------------------------------------------------------------------------

bool TMainForm::DoCalibration(AnsiString &err)
{
	Application->ProcessMessages();

	// get the parameter ranges and store them in PMatSD and PMatIT
	if (!ParameterRangesManipulated)
		if (!DeriveParamRangesFromData())
			return false;

	TParameterMatrix pmat_sd_from_gui;
	TParameterMatrix pmat_it_from_gui;

	// SD
	if (!LoadParamDataFromGUI(pmat_sd_from_gui, gutsSD))
		return false;
	for (int p = 0; p < gutsPARAMETER_NO; p++)
	{
		if (pmat_sd_from_gui.Min[p] != MISSING_DATA_NUM && pmat_sd_from_gui.Min[p] >= PMatSD.Min[p])
			PMatSD.Min[p] = pmat_sd_from_gui.Min[p];

		if (pmat_sd_from_gui.Max[p] != MISSING_DATA_NUM && pmat_sd_from_gui.Max[p] >= PMatSD.Min[p])
			PMatSD.Max[p] = pmat_sd_from_gui.Max[p];

		if (pmat_sd_from_gui.Value[p] < PMatSD.Max[p] + EPS && pmat_sd_from_gui.Value[p] > PMatSD.Min[p] - EPS)
			PMatSD.Value[p] = pmat_sd_from_gui.Value[p];
		else
			PMatSD.Value[p] = PMatSD.Max[p];
	}
	PMatSD.Fit = pmat_sd_from_gui.Fit;
	PMatSD.Scale = pmat_sd_from_gui.Scale;

	// IT
	if (!LoadParamDataFromGUI(pmat_it_from_gui, gutsIT))
		return false;
	for (int p = 0; p < gutsPARAMETER_NO; p++)
	{
		if (pmat_it_from_gui.Min[p] != MISSING_DATA_NUM && pmat_it_from_gui.Min[p] >= PMatIT.Min[p])
			PMatIT.Min[p] = pmat_it_from_gui.Min[p];

		if (pmat_it_from_gui.Max[p] != MISSING_DATA_NUM && pmat_it_from_gui.Max[p] >= PMatIT.Min[p])
			PMatIT.Max[p] = pmat_it_from_gui.Max[p];

		if (pmat_it_from_gui.Value[p] < PMatIT.Max[p] + EPS && pmat_it_from_gui.Value[p] > PMatIT.Min[p] - EPS)
			PMatIT.Value[p] = pmat_it_from_gui.Value[p];
		else
			PMatIT.Value[p] = PMatIT.Max[p];
	}
	PMatIT.Fit = pmat_it_from_gui.Fit;
	PMatIT.Scale = pmat_it_from_gui.Scale;

	// update the displayed matrices
	bool change_value = true;
	bool change_range = false;
	DisplayParameterMatrixInGUI(PMatSD, gutsSD, change_value, change_range);
	DisplayParameterMatrixInGUI(PMatIT, gutsIT, change_value, change_range);

	// first go to SD output tab to see memo
	CalibrationPageControl->ActivePage = CalibrationOutputTab;
	CalibrationOutputPageControl->ActivePage = CalibrationSDOutputTab;
	CalibrationSDOutputPageControl->ActivePage = CalibrationSDOutputParamSpaceTab;
	// then do the optimisation for SD
	CalibrationPerformed = false;

	if (!Calc_Optim(PMatPrintSD, Calib_MLL_SD, PMatSD, gutsSD, optimREGULAR_OPTIM_FLAG, err))
		return false;
	if (AbortCalibration)
		return true;

	// first go to IT output tab to see memo
	CalibrationOutputPageControl->ActivePage = CalibrationITOutputTab;
	CalibrationITOutputPageControl->ActivePage = CalibrationITOutputParamSpaceTab;
	// then do the optimisation for IT
	CalibrationPerformed = false;
	// CalibrationPerformed is set true during gutsSD optimisation
	if (!Calc_Optim(PMatPrintIT, Calib_MLL_IT, PMatIT, gutsIT, optimREGULAR_OPTIM_FLAG, err))
		return false;
	if (AbortCalibration)
		return true;

	CalibrationPerformed = true;

	return true;
}
// ---------------------------------------------------------------------------

void TMainForm::UpdateParameterValuesInGUI()
{
    Application->ProcessMessages();

	for (int p = 0; p < gutsPARAMETER_NO; p++)
	{
		if (p != gutsFS)
			GUTSSDParamValueEdits[p]->Text = MyFloatToStr(PMatPrintSD.BestValue[p], 4);
		if (p != gutsBW)
			GUTSITParamValueEdits[p]->Text = MyFloatToStr(PMatPrintIT.BestValue[p], 4);
	}

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::DeleteOldImages(TScrollBox *scrollbox)
{
	for (int i = scrollbox->ComponentCount - 1; i >= 0; i--)
	{
		TImage *img = (TImage*)scrollbox->Components[i];
		delete img;
	}

	return;
}
// ---------------------------------------------------------------------------

vector<double>TMainForm::EquidistantNumbers(double a, double b, int n)
	// returns a vector with n numbers of equal spacing in the interval [a; b]
{
	Application->ProcessMessages();

	vector<double>out;
	out.resize(0);

	if (n <= 1)
		return out; // return empty vector in case of small n

	double spacing;

	if (n - 1.0 > EPS && b - a > EPS)
		spacing = (b - a) / (n - 1);
	else
		spacing = 0.0;
	for (int m = 0; m < n; m++)
		out.push_back(a + m*spacing);

	return out;
}
// ---------------------------------------------------------------------------

double TMainForm::MaxNumInVec(vector<double> &in)
	// returns the highest number in a vector
{
	if (in.size() < 1)
		return 0.0;

	double out = HIGH_NEG_DOUBLE;
	for (int i = 0; i < in.size(); i++)
		if (in[i] > out)
			out = in[i];

	return out;
}
// ---------------------------------------------------------------------------

void TMainForm::PaintValidationOutput(int sdit)
{

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::PaintGUTSGraphs(TPreparedDataContainer &data_all, int number_of_sets, double hb_fix, TScrollBox *scrollbox,
	TImage *batch_img, int stage, int sdit, AnsiString &err)
{
	Application->ProcessMessages();

	// plot the output ...

	// FIRST PREPARE DATA FOR PLOTTING (CALCULATE DATA TO BE PLOTTED AND TRANSFORM IT)
	// input parameters for Calc_Conf()
	int flag_conf = 1; // 1 = calculate CIs, 0 = don't
	if (stage == PREDICTION || stage == BATCH_PREDICTION)
	{
		flag_conf = 0;
		hb_fix = 0.0;
	}

	// get maximum time point of data sets and groups
	vector<double> t_max = GetMaximumTimePointsFromPreparedData(data_all);

	// add all time points from measured data and make vector unique
	// record data for observed vs predicted plot on the fly
	vector<double>t_temp;
	if (stage != PREDICTION && stage != BATCH_PREDICTION)
	{
		PredictedVsObserved_ObservedTimes.resize(number_of_sets);
		PredictedVsObserved_ObservedSurv.resize(number_of_sets);
	}
	vector<vector<double> > sim_time;
	sim_time.resize(number_of_sets);
	for (int d = 0; d < number_of_sets; d++)
	{
		// get simulation time vector with equally distributed numbers
		sim_time[d] = EquidistantNumbers(0.0, t_max[d], Max(plotT_PLOT[0], t_max[d] * plotT_PLOT[1]));
		// time points for which the output should be calculated

		if (stage != PREDICTION && stage != BATCH_PREDICTION)
		{
			PredictedVsObserved_ObservedTimes[d].resize(data_all.Survival[d].size());
			PredictedVsObserved_ObservedSurv[d].resize(data_all.Survival[d].size());
		}
		for (int g = 0; g < data_all.Survival[d].size(); g++)
		{
			t_temp = data_all.Survival[d][g].Times;
			sim_time[d].insert(sim_time[d].end(), t_temp.begin(), t_temp.end());
			if (stage != PREDICTION && stage != BATCH_PREDICTION)
			{
				PredictedVsObserved_ObservedTimes[d][g] = t_temp;
				PredictedVsObserved_ObservedSurv[d][g] = data_all.Survival[d][g].Survivors;
			}
		}
		sort(sim_time[d].begin(), sim_time[d].end());
		sim_time[d].erase(unique(sim_time[d].begin(), sim_time[d].end()), sim_time[d].end());
	}

	vector<TSimulationOutput>data_set_out;

	DeleteOldImages(scrollbox); // Delete old graphs

	if (stage != PREDICTION && stage != BATCH_PREDICTION)
	{
		PredictedVsObserved_PredictedTimes.resize(number_of_sets);
		PredictedVsObserved_PredictedSurv.resize(number_of_sets);
		PredictedVsObserved_PredictedSurv_LowerCI.resize(number_of_sets);
		PredictedVsObserved_PredictedSurv_UpperCI.resize(number_of_sets);
	}
	// calculate confidence bands and paint graphs
	TImage* img;
	for (int d = 0; d < number_of_sets; d++)
	{

		// calculation of plot data
		Calc_Conf_Per_Data_Set(data_set_out, sim_time[d], data_all.Concentration[d], sdit, hb_fix, flag_conf, err);
		if (err != "")
			return;

		// store data for the predicted vs survival plotting
		if (stage != PREDICTION && stage != BATCH_PREDICTION)
		{
			PredictedVsObserved_PredictedTimes[d].resize(data_set_out.size());
			PredictedVsObserved_PredictedSurv[d].resize(data_set_out.size());
			PredictedVsObserved_PredictedSurv_LowerCI[d].resize(data_set_out.size());
			PredictedVsObserved_PredictedSurv_UpperCI[d].resize(data_set_out.size());
			for (int g = 0; g < data_set_out.size(); g++)
			{
				PredictedVsObserved_PredictedTimes[d][g] = data_set_out[g].Times;
				PredictedVsObserved_PredictedSurv[d][g] = data_set_out[g].SurvivalProb;
				PredictedVsObserved_PredictedSurv_LowerCI[d][g] = data_set_out[g].SurvivalProbLowerCI;
				PredictedVsObserved_PredictedSurv_UpperCI[d][g] = data_set_out[g].SurvivalProbUpperCI;
			}
		}

		// DO THE ACTUAL PLOTTING OF THE GUTS GRAPHS
		bool display_exposure = true;
		bool display_damage = true;
		bool display_predicted_survival = true;
		bool display_measured_survival = true;
		if (stage == PREDICTION || stage == BATCH_PREDICTION)
			display_measured_survival = false;

		img = new TImage(scrollbox);
		if (stage != BATCH_PREDICTION)
			img->Parent = scrollbox;
		AdjustImageDimensions(data_all, img, display_exposure, display_damage, display_measured_survival,
			display_predicted_survival);
		DisplayGUTSGraphs(data_all, data_set_out, d, img, display_exposure, display_damage, display_measured_survival,
			display_predicted_survival, stage);
		if (stage != BATCH_PREDICTION)
		{
			scrollbox->HorzScrollBar->Range = Max(img->Width, scrollbox->HorzScrollBar->Range);
			scrollbox->VertScrollBar->Range = Max(img->Height * number_of_sets, scrollbox->VertScrollBar->Range);
		}
		else
		{
			if (sdit == gutsSD)
			{
				BatchPredictionSurvivalImageSD->Width = img->Width;
				BatchPredictionSurvivalImageSD->Height = img->Height;
				BatchPredictionSurvivalImageSD->Picture->Assign(img->Picture);
			}
			else
			{
				BatchPredictionSurvivalImageIT->Width = img->Width;
				BatchPredictionSurvivalImageIT->Height = img->Height;
				BatchPredictionSurvivalImageIT->Picture->Assign(img->Picture);
			}
		}
	}
	// sort the images in the scrollbox in the correct order
	if (stage != BATCH_PREDICTION)
	{
		for (int i = scrollbox->ComponentCount - 1; i >= 0; i--)
		{
			TImage *img_tmp = (TImage*)scrollbox->Components[i];
			img_tmp->Align = alTop;
		}
	}

	if (stage != PREDICTION && stage != BATCH_PREDICTION)
	{
		// adjust the predicted data to the observed data (extract the needed time values)
		for (int d = 0; d < number_of_sets; d++)
		{
			for (int g = 0; g < PredictedVsObserved_PredictedTimes[d].size(); g++)
			{
				vector<double> S_req; // data vector for requested time points
				vector<double> S_low_req; // data vector for requested time points
				vector<double> S_high_req; // data vector for requested time points
				int t_rem_it = 0;
				for (int i = 0; i < PredictedVsObserved_PredictedTimes[d][g].size(); i++)
					if (PredictedVsObserved_PredictedTimes[d][g][i] == PredictedVsObserved_ObservedTimes[d][g][t_rem_it])
						// find where the requested time points are in the longer <t> (vector with indices)
					{
						S_req.push_back(PredictedVsObserved_PredictedSurv[d][g][i]);
						S_low_req.push_back(PredictedVsObserved_PredictedSurv_LowerCI[d][g][i]);
						S_high_req.push_back(PredictedVsObserved_PredictedSurv_UpperCI[d][g][i]);
						t_rem_it++;
						if (t_rem_it == PredictedVsObserved_ObservedTimes[d][g].size())
							break; // break loop if last t_rem element is reached
						i--; // if there are duplicate time points in t_rem, then t[i] has to be checked again
					}

				// only keep the rows we asked for
				if (PredictedVsObserved_ObservedTimes[d][g].size() == S_req.size())
				{
					PredictedVsObserved_PredictedTimes[d][g] = PredictedVsObserved_ObservedTimes[d][g];
					PredictedVsObserved_PredictedSurv[d][g] = S_req;
					PredictedVsObserved_PredictedSurv_LowerCI[d][g] = S_low_req;
					PredictedVsObserved_PredictedSurv_UpperCI[d][g] = S_high_req;
				}
			}
		}

		// PREDICTED VS OBSERVED PAINTING
		PaintPredictionVSObservedGraphs(stage, sdit);
	}

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::PaintPredictionVSObservedGraphs(int stage, int sdit)
{
    Application->ProcessMessages();

	TImage *img_vs;
	TImage *img_vs_death;
	if (stage == CALIBRATION)
	{
		if (sdit == gutsSD)
		{
			img_vs = CalibrationSDOutputObsVSPredSurvivorsImage;
			img_vs_death = CalibrationSDOutputObsVSPredDeathsImage;
		}
		else
		{
			img_vs = CalibrationITOutputObsVSPredSurvivorsImage;
			img_vs_death = CalibrationITOutputObsVSPredDeathsImage;
		}
		// predicted vs observed survival graph
		DisplayPredictedVsObservedGraph(CalibData_All, img_vs, sdit);
		// predicted vs observed deaths graph
		PreparePredictedVsObserved_DeathData(sdit);
		DisplayPredictedVsObservedGraph_Deaths(CalibData_All, img_vs_death, sdit);
	}
	if (stage == VALIDATION)
	{
		if (sdit == gutsSD)
		{
			img_vs = ValidationSDOutputObsVSPredSurvivorsImage;
			img_vs_death = ValidationSDOutputObsVSPredDeathsImage;
		}
		else
		{
			img_vs = ValidationITOutputObsVSPredSurvivorsImage;
			img_vs_death = ValidationITOutputObsVSPredDeathsImage;
		}
		// predicted vs observed survival graph
		DisplayPredictedVsObservedGraph(ValidationData_All, img_vs, sdit);
		// predicted vs observed deaths graph
		PreparePredictedVsObserved_DeathData(sdit);
		DisplayPredictedVsObservedGraph_Deaths(ValidationData_All, img_vs_death, sdit);
	}

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::DisplayGoodnessOfFit(TPreparedDataContainer &data_all, TMemo *memo, int sdit, int stage)
{
    Application->ProcessMessages();

	AnsiString as;
	TGoodnessOfFit gof;

	// Do an r-square calculation on the survival *probabilities*. This is
	// also known as the 'model efficiency'.
	double ssq_fit = 0;
	double mean_obs = 0;
	int count = 0;
	for (int d = 0; d < PredictedVsObserved_ObservedSurv.size(); d++)
		for (int g = 0; g < PredictedVsObserved_ObservedSurv[d].size(); g++)
			for (int t = 1; t < PredictedVsObserved_ObservedSurv[d][g].size(); t++)
				if (PredictedVsObserved_ObservedSurv[d][g][t] != MISSING_DATA_NUM)
				{
					ssq_fit += pow(PredictedVsObserved_ObservedSurv[d][g][t] / PredictedVsObserved_ObservedSurv[d][g][0] -
							PredictedVsObserved_PredictedSurv[d][g][t], 2.0);
					mean_obs += PredictedVsObserved_ObservedSurv[d][g][t] / PredictedVsObserved_ObservedSurv[d][g][0];
					count++;
				}
	mean_obs /= count;
	double ssq_tot = 0;
	for (int d = 0; d < PredictedVsObserved_ObservedSurv.size(); d++)
		for (int g = 0; g < PredictedVsObserved_ObservedSurv[d].size(); g++)
			for (int t = 1; t < PredictedVsObserved_ObservedSurv[d][g].size(); t++)
				if (PredictedVsObserved_ObservedSurv[d][g][t] != MISSING_DATA_NUM)
				{
					ssq_tot += pow(PredictedVsObserved_ObservedSurv[d][g][t] / PredictedVsObserved_ObservedSurv[d][g][0] -
							mean_obs, 2.0);
				}
	double r_square = 1.0 - ssq_fit / ssq_tot;

	// ssq_fit  = sum((coll_p(:,1)-coll_p(:,2)).^2); // sum-of-squares of the residuals
	// ssq_tot  = sum((coll_p(:,1)-mean(coll_p(:,1))).^2); // sum-of-squares of the total (compared to mean)
	// r_square = 1 - ssq_fit / ssq_tot; // r-square

	// Do NRMSE as in EFSA document on survival *numbers*, expressed as percentage
	ssq_fit = 0;
	mean_obs = 0;
	count = 0;
	for (int d = 0; d < PredictedVsObserved_ObservedSurv.size(); d++)
		for (int g = 0; g < PredictedVsObserved_ObservedSurv[d].size(); g++)
			for (int t = 1; t < PredictedVsObserved_ObservedSurv[d][g].size(); t++)
				if (PredictedVsObserved_ObservedSurv[d][g][t] != MISSING_DATA_NUM)
				{
					ssq_fit +=
						pow(PredictedVsObserved_ObservedSurv[d][g][t] -
						PredictedVsObserved_PredictedSurv[d][g][t] * PredictedVsObserved_ObservedSurv[d][g][0], 2.0);
					mean_obs += PredictedVsObserved_ObservedSurv[d][g][t];
					count++;
				}
	mean_obs /= count;
	double nrmse = 100.0 * pow(ssq_fit / count, 0.5) / mean_obs;

	// ssq_fit = sum((coll_s(:,1)-coll_s(:,2)).^2); // sum-of-squares of the residuals
	// nrmse   = 100 * sqrt(ssq_fit/length(coll_s(:,1))) / mean(coll_s(:,1));

	// Do SPPE as in EFSA document. Expressed as percentage. This is the
	// deviation between observed and predicted survival *probability* at
	// the end of the test, for each treatment. The first two columns are
	// for the number of the data set and the number of the treatment.
	vector<AnsiString>sppe_vecs;
	// vector<double> sppe;
	double last_obs_val, last_pred_val;
	for (int d = 0; d < PredictedVsObserved_ObservedSurv.size(); d++)
		for (int g = 0; g < PredictedVsObserved_ObservedSurv[d].size(); g++)
		{
			for (int t = 1; t < PredictedVsObserved_ObservedSurv[d][g].size(); t++)
			{
				if (PredictedVsObserved_ObservedSurv[d][g][t] != MISSING_DATA_NUM)
				{
					last_obs_val = PredictedVsObserved_ObservedSurv[d][g][t] / PredictedVsObserved_ObservedSurv[d][g][0];
					last_pred_val = PredictedVsObserved_PredictedSurv[d][g][t];
				}
			}
			sppe_vecs.push_back(AnsiString(d + 1) + "\t" + data_all.GroupNames[d][g] + "\t" +
				MyFloatToStr(100.0 * (last_obs_val - last_pred_val), 4) + " %");
		}

	// sppe = [coll_p_end(:,[3 4]) 100 * (coll_p_end(:,1) - coll_p_end(:,2))];

	// Finally, put them on-screen in a formatted manner.
	memo->Lines->Add("Goodness of fit measures");
	if (sdit == gutsSD)
		memo->Lines->Add("  Special case: stochastic death (SD)");
	else
		memo->Lines->Add("  Special case: individual tolerance (IT)");

	memo->Lines->Add("=====================================================================");
	as = "Model efficiency (NSE, r-square): " + MyFloatToStr(r_square, 4);
	memo->Lines->Add(as);
	as = "Normalised root-means-square error (NRMSE): " + MyFloatToStr(nrmse, 4) + " %";
	memo->Lines->Add(as);
	memo->Lines->Add("Survival probability prediction error (SPPE) for each treatment:");
	memo->Lines->Add("Data set\ttreatment\tvalue");
	for (int e = 0; e < sppe_vecs.size(); e++)
		memo->Lines->Add(sppe_vecs[e]);

	memo->Lines->Add("=====================================================================");
	memo->Lines->Add("Warning: these measures need to be interpreted more qualitatively as they, strictly speaking, do not apply to quantal data");

	gof.R_square = MyFloatToStr(r_square, 4);
	gof.NRMSE = MyFloatToStr(nrmse, 4);
	gof.SPPE = sppe_vecs;

	if (stage == CALIBRATION)
	{
		if (sdit == gutsSD)
		{
			CalibrationSDGoodnessOfFit.R_square = gof.R_square;
			CalibrationSDGoodnessOfFit.NRMSE = gof.NRMSE;
			CalibrationSDGoodnessOfFit.SPPE = gof.SPPE;
		}
		else
		{
			CalibrationITGoodnessOfFit.R_square = gof.R_square;
			CalibrationITGoodnessOfFit.NRMSE = gof.NRMSE;
			CalibrationITGoodnessOfFit.SPPE = gof.SPPE;
		}
	}
	else
	{
		if (sdit == gutsSD)
			ValidationSDGoodnessOfFit = gof;
		else
			ValidationITGoodnessOfFit = gof;
	}

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::PreparePredictedVsObserved_DeathData(int sdit)
{
    Application->ProcessMessages();

	double test1, test2;
	PredictedVsObserved_ObservedDeaths.resize(PredictedVsObserved_ObservedTimes.size());
	PredictedVsObserved_PredictedDeaths.resize(PredictedVsObserved_ObservedTimes.size());
	for (int d = 0; d < PredictedVsObserved_ObservedTimes.size(); d++)
	{
		PredictedVsObserved_ObservedDeaths[d].resize(PredictedVsObserved_ObservedTimes[d].size());
		PredictedVsObserved_PredictedDeaths[d].resize(PredictedVsObserved_ObservedTimes[d].size());
		for (int g = 0; g < PredictedVsObserved_ObservedTimes[d].size(); g++)
		{
			PredictedVsObserved_ObservedDeaths[d][g].resize(0);
			PredictedVsObserved_PredictedDeaths[d][g].resize(0);
			for (int t = 1; t < PredictedVsObserved_ObservedTimes[d][g].size(); t++)
			{
				PredictedVsObserved_ObservedDeaths[d][g].push_back(PredictedVsObserved_ObservedSurv[d][g][t - 1] -
					PredictedVsObserved_ObservedSurv[d][g][t]);
				PredictedVsObserved_PredictedDeaths[d][g].push_back
					((PredictedVsObserved_PredictedSurv[d][g][t - 1] - PredictedVsObserved_PredictedSurv[d][g][t])
					* PredictedVsObserved_ObservedSurv[d][g][0]);
			}
			test1 = PredictedVsObserved_ObservedSurv[d][g][PredictedVsObserved_ObservedSurv[d][g].size() - 1];
			test2 = PredictedVsObserved_PredictedSurv[d][g][PredictedVsObserved_PredictedSurv[d][g].size() - 1];
			PredictedVsObserved_ObservedDeaths[d][g].push_back
				(PredictedVsObserved_ObservedSurv[d][g][PredictedVsObserved_ObservedSurv[d][g].size() - 1]);
			PredictedVsObserved_PredictedDeaths[d][g].push_back
				(PredictedVsObserved_PredictedSurv[d][g][PredictedVsObserved_PredictedSurv[d][g].size() - 1]
				* PredictedVsObserved_ObservedSurv[d][g][0]);
		}
	}

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::DisplayPredictedVsObservedGraph_Deaths(TPreparedDataContainer &data_all, TImage *img, int sdit)
{
    Application->ProcessMessages();

	// setting up the chart
	TChart *chart = new TChart(this);
	chart->AllowPanning = Vcltee::Teeprocs::pmNone;
	chart->BevelOuter = bvNone;
	chart->Color = clWhite;
	chart->View3D = false;
	chart->View3DWalls = false;
	chart->Legend->Visible = true;
	chart->Legend->Shadow->Visible = false;

	chart->LeftAxis->Grid->Visible = false;
	chart->LeftAxis->Axis->Width = 1;
	chart->LeftAxis->AxisValuesFormat = "#,##0.0";
	chart->LeftAxis->Texts->Font->Color = clBlack;
	chart->LeftAxis->Title->Font->Size = 12;
	chart->LeftAxis->Title->Text = "Predicted";

	chart->BottomAxis->Grid->Visible = false;
	chart->BottomAxis->Axis->Width = 1;
	chart->BottomAxis->AxisValuesFormat = "#,##0.0";
	chart->BottomAxis->Texts->Font->Color = clBlack;
	chart->BottomAxis->Title->Font->Size = 12;
	chart->BottomAxis->Title->Text = "Observed";

	int max_val = 0;
	for (int d = 0; d < PredictedVsObserved_ObservedTimes.size(); d++)
		for (int g = 0; g < PredictedVsObserved_ObservedTimes[d].size(); g++)
			for (int t = 0; t < PredictedVsObserved_ObservedTimes[d][g].size(); t++)
			{
				if (PredictedVsObserved_ObservedDeaths[d][g][t] > max_val)
					max_val = PredictedVsObserved_ObservedDeaths[d][g][t];
				if (PredictedVsObserved_PredictedDeaths[d][g][t] > max_val)
					max_val = PredictedVsObserved_PredictedDeaths[d][g][t];
			}

	// setting up the series
	chart->RemoveAllSeries();

	chart->AddSeries(new TLineSeries(chart));
	TLineSeries* ls1 = (TLineSeries*)chart->Series[0];
	ls1->LinePen->Style = psDash;
	ls1->SeriesColor = clBlack;
	ls1->LinePen->Width = 2;
	chart->Series[chart->SeriesCount() - 1]->Legend->Text = "1:1 line";
	chart->Series[0]->AddXY(0, 0);
	chart->Series[0]->AddXY(max_val, max_val);

	chart->AddSeries(new TLineSeries(chart));
	TLineSeries* ls2 = (TLineSeries*)chart->Series[1];
	ls2->LinePen->Style = psDot;
	ls2->SeriesColor = clMedGray;
	chart->Series[chart->SeriesCount() - 1]->Legend->Text = "20 % deviation";
	chart->Series[1]->AddXY(0, 0.2*max_val);
	chart->Series[1]->AddXY(0.8*max_val, max_val);

	chart->AddSeries(new TLineSeries(chart));
	TLineSeries* ls3 = (TLineSeries*)chart->Series[2];
	ls3->LinePen->Style = psDot;
	ls3->SeriesColor = clMedGray;
	chart->Series[chart->SeriesCount() - 1]->Legend->Visible = false;
	chart->Series[2]->AddXY(0.2*max_val, 0);
	chart->Series[2]->AddXY(max_val, 0.8*max_val);

	for (int d = 0; d < PredictedVsObserved_PredictedTimes.size(); d++)
		for (int g = 0; g < PredictedVsObserved_PredictedTimes[d].size(); g++)
		{
			chart->AddSeries(new TPointSeries(chart));
			TPointSeries* ps = (TPointSeries*)chart->Series[chart->SeriesCount() - 1];
			ps->Pointer->Size = 4;
			for (int i = 0; i < PredictedVsObserved_PredictedTimes[d][g].size(); i++)
			{
				chart->Series[chart->SeriesCount() - 1]->AddXY(PredictedVsObserved_ObservedDeaths[d][g][i],
					PredictedVsObserved_PredictedDeaths[d][g][i]);
			}
			chart->Series[chart->SeriesCount() - 1]->Legend->Text =
				("Set " + AnsiString(d + 1) + ", " + data_all.GroupNames[d][g]).c_str();
		}

	TRect rect;
	rect.Left = 0;
	rect.Top = 0;
	rect.Right = 1.5 * plotSINGLE_CHART_WIDTH;
	rect.Bottom = 1.0 * plotSINGLE_CHART_HEIGHT;

	img->Picture->Assign(chart->TeeCreateMetafile(true, rect));

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::DisplayPredictedVsObservedGraph(TPreparedDataContainer &data_all, TImage *img, int sdit)
{
    Application->ProcessMessages();

	// setting up the chart
	TChart *chart = new TChart(this);
	chart->AllowPanning = Vcltee::Teeprocs::pmNone;
	chart->BevelOuter = bvNone;
	chart->Color = clWhite;
	chart->View3D = false;
	chart->View3DWalls = false;
	chart->Legend->Visible = true;
	chart->Legend->Shadow->Visible = false;

	chart->LeftAxis->Grid->Visible = false;
	chart->LeftAxis->Axis->Width = 1;
	chart->LeftAxis->AxisValuesFormat = "#,##0.0";
	chart->LeftAxis->Texts->Font->Color = clBlack;
	chart->LeftAxis->Title->Font->Size = 12;
	chart->LeftAxis->Title->Text = "Predicted";

	chart->BottomAxis->Grid->Visible = false;
	chart->BottomAxis->Axis->Width = 1;
	chart->BottomAxis->AxisValuesFormat = "#,##0.0";
	chart->BottomAxis->Texts->Font->Color = clBlack;
	chart->BottomAxis->Title->Font->Size = 12;
	chart->BottomAxis->Title->Text = "Observed";

	// setting up the series
	chart->RemoveAllSeries();

	chart->AddSeries(new TLineSeries(chart));
	TLineSeries* ls1 = (TLineSeries*)chart->Series[0];
	ls1->LinePen->Style = psDash;
	ls1->SeriesColor = clBlack;
	ls1->LinePen->Width = 2;
	chart->Series[chart->SeriesCount() - 1]->Legend->Text = "1:1 line";
	chart->Series[0]->AddXY(0, 0);
	chart->Series[0]->AddXY(1, 1);

	chart->AddSeries(new TLineSeries(chart));
	TLineSeries* ls2 = (TLineSeries*)chart->Series[1];
	ls2->LinePen->Style = psDot;
	ls2->SeriesColor = clMedGray;
	chart->Series[chart->SeriesCount() - 1]->Legend->Text = "20 % deviation";
	chart->Series[1]->AddXY(0, 0.2);
	chart->Series[1]->AddXY(0.8, 1);

	chart->AddSeries(new TLineSeries(chart));
	TLineSeries* ls3 = (TLineSeries*)chart->Series[2];
	ls3->LinePen->Style = psDot;
	ls3->SeriesColor = clMedGray;
	chart->Series[chart->SeriesCount() - 1]->Legend->Visible = false;
	chart->Series[2]->AddXY(0.2, 0);
	chart->Series[2]->AddXY(1, 0.8);

	int series_legend_index = 0;
	for (int d = 0; d < PredictedVsObserved_PredictedTimes.size(); d++)
		for (int g = 0; g < PredictedVsObserved_PredictedTimes[d].size(); g++)
		{
			chart->AddSeries(new TPointSeries(chart));
			TPointSeries* ps = (TPointSeries*)chart->Series[chart->SeriesCount() - 1];
			ps->Pointer->Size = 4;
			series_legend_index = chart->SeriesCount() - 1; // remember the series to draw with legend
			for (int i = 0; i < PredictedVsObserved_PredictedTimes[d][g].size(); i++)
			{
				chart->Series[series_legend_index]->AddXY
					(PredictedVsObserved_ObservedSurv[d][g][i] / PredictedVsObserved_ObservedSurv[d][g][0],
					PredictedVsObserved_PredictedSurv[d][g][i]);
				// paint error bars
				if (i > 0)
				{
					chart->AddSeries(new TLineSeries(chart));
					chart->Series[chart->SeriesCount() - 1]->Color = clBlack;
					chart->Series[chart->SeriesCount() - 1]->AddXY(
						PredictedVsObserved_ObservedSurv[d][g][i] / PredictedVsObserved_ObservedSurv[d][g][0],
						PredictedVsObserved_PredictedSurv_LowerCI[d][g][i]);
					chart->Series[chart->SeriesCount() - 1]->AddXY(
						PredictedVsObserved_ObservedSurv[d][g][i] / PredictedVsObserved_ObservedSurv[d][g][0],
						PredictedVsObserved_PredictedSurv_UpperCI[d][g][i]);
					chart->Series[chart->SeriesCount() - 1]->Legend->Visible = false;
				}
			}
			chart->Series[series_legend_index]->Legend->Text =
				("Set " + AnsiString(d + 1) + ", " + data_all.GroupNames[d][g]).c_str();
		}

	TRect rect;
	rect.Left = 0;
	rect.Top = 0;
	rect.Right = 1.5 * plotSINGLE_CHART_WIDTH;
	rect.Bottom = 1.0 * plotSINGLE_CHART_HEIGHT;
	img->Picture->Assign(chart->TeeCreateMetafile(true, rect));

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::DisableGUI()
{
	InputTab->Enabled = false;
	CalibrationTab->Enabled = false;
	ValidationTab->Enabled = false;
	PredictionsTab->Enabled = false;
	DisableMainMenu();

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::EnableGUI()
{
	InputTab->Enabled = true;
	CalibrationTab->Enabled = true;
	ValidationTab->Enabled = true;
	PredictionsTab->Enabled = true;
	EnableMainMenu();

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::DisableMainMenu()
{
	ToolBar1->Enabled = false;
	Batchpredictions1->Enabled = false;
	File1->Enabled = false;
	Help1->Enabled = false;
	Reprot1->Enabled = false;

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::EnableMainMenu()
{
	ToolBar1->Enabled = true;
	Batchpredictions1->Enabled = true;
	File1->Enabled = true;
	Help1->Enabled = true;
	Reprot1->Enabled = true;

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::DisableGUIForCalibration()
{
	CalibrationPageControl->Enabled = false;
	ExpertSettingsPageControl->Enabled = false;
	StartCalibrationButton->Enabled = false;
	ShowExpertSettingsCheckBox->Enabled = false;
	FitHBToControlCheckBox->Enabled = false;
	InputTab->Enabled = false;
	ValidationTab->Enabled = false;
	PredictionsTab->Enabled = false;
	DisableMainMenu();

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::EnableGUIAfterCalibration()
{
	CalibrationPageControl->Enabled = true;
	ExpertSettingsPageControl->Enabled = true;
	StartCalibrationButton->Enabled = true;
	ShowExpertSettingsCheckBox->Enabled = true;
	FitHBToControlCheckBox->Enabled = true;
	InputTab->Enabled = true;
	ValidationTab->Enabled = true;
	PredictionsTab->Enabled = true;
	EnableMainMenu();

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::DisableGUIForPredictions()
{
	PredictionsPageControl->Enabled = false;
	PredictionsLeftUpperPanel->Enabled = false;
	PredictionsExposureProfilePanel->Enabled = false;
	StartSimulationButton->Enabled = false;
	DisplayPredictionsDataButton->Visible = false;
	AbortPredictionsButton->Enabled = true;
	PredictionsProgressBar->Visible = true;
	LPxCalcCICheckBox->Enabled = false;
	InputTab->Enabled = false;
	ValidationTab->Enabled = false;
	CalibrationTab->Enabled = false;
	DisableMainMenu();

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::EnableGUIAfterPredictions()
{
	PredictionsPageControl->Enabled = true;
	PredictionsLeftUpperPanel->Enabled = true;
	PredictionsExposureProfilePanel->Enabled = true;
	StartSimulationButton->Enabled = true;
	DisplayPredictionsDataButton->Visible = true;
	AbortPredictionsButton->Enabled = false;
	PredictionsProgressBar->Visible = false;
	LPxCalcCICheckBox->Enabled = true;
	InputTab->Enabled = true;
	ValidationTab->Enabled = true;
	CalibrationTab->Enabled = true;
	EnableMainMenu();

	return;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::StartCalibrationButtonClick(TObject *Sender)
	// do a SD and a IT calibration for the calibration data
{
	Application->ProcessMessages();

	Screen->Cursor = crHourGlass;
	AbortCalibration = false;
	CalibrationStatusLabel->Font->Color = clBlue;
	CalibrationStatusLabel->Caption = "Calibration running...";
	CalibrationSDOutputPageControl->Visible = false;
	CalibrationITOutputPageControl->Visible = false;

	// disable the tab
	DisableGUIForCalibration();

	// CHECK and GET calibration data
	if (!CheckAndGetCalibrationData(CalibrationData))
	{
		EnableGUIAfterCalibration();
		Screen->Cursor = crDefault;
		return;
	}

	CalibrationSDMemo->Clear();
	CalibrationITMemo->Clear();
	CalibrationPerformed = false;
	CalibrationResultsPlotted = false;

	// clear validation and prediction
	ClearValidationResults();
	ValidationResultsPlotted = false;
	ClearPredictionsResults();

	// start calibration for both: SD and IT
	AnsiString err = "";
	if (!DoCalibration(err))
	{
		// ToDoAfterInputDataManipulation();
		if (err != "")
			ShowMessage(err);
		EnableGUIAfterCalibration();
		Reset(false);
		Screen->Cursor = crDefault;
		return;
	}
	if (AbortCalibration)
	{
		CalibrationStatusLabel->Font->Color = clRed;
		CalibrationStatusLabel->Caption = "Calibration aborted!";
		CalibrationSDMemo->Clear();
		CalibrationSDMemo->Lines->Add("Calibration aborted!");
		CalibrationITMemo->Clear();
		CalibrationITMemo->Lines->Add("Calibration aborted!");
		EnableGUIAfterCalibration();
		Screen->Cursor = crDefault;
		CalibrationLeftPanel->Enabled = true;
		AbortCalibration = false;
		return;
	}

	CalibrationPerformed = true;

	UpdateParameterValuesInGUI();

	// Paint the calibration results
	PaintParameterSpacePlot();
	double hb_fix = -1.0;
	// identifyer (don't fix background mortality in sample)

	TScrollBox *scrollbox;
	TMemo *memo;
	TImage *dummy_img;
	AnsiString error = "";
	for (int sdit = gutsSD; sdit <= gutsIT; sdit++) // for gutsSD and gutsIT
	{
		if (sdit == gutsSD)
		{
			scrollbox = CalibSDSurvivalScrollBox;
			memo = CalibrationSDMemo;
		}
		else
		{
			scrollbox = CalibITSurvivalScrollBox;
			memo = CalibrationITMemo;
		}

		PaintGUTSGraphs(CalibData_All, NumberOfCalibrationDataSets, hb_fix, scrollbox, dummy_img, CALIBRATION, sdit, error);
		if (error != "")
		{
			ShowMessage(error);
			Reset(false);
			EnableGUIAfterCalibration();
			Screen->Cursor = crDefault;
			return;
		}

		// write GOODNESS OF FIT to memo
		DisplayGoodnessOfFit(CalibData_All, memo, sdit, CALIBRATION);
	}
	CalibrationResultsPlotted = true;

	CalibrationStatusLabel->Font->Color = clGreen;
	CalibrationStatusLabel->Caption = "Calibration finished!";
	//CalibrationLeftPanel->Enabled = true;
	CalibrationSDOutputPageControl->Visible = true;
	CalibrationITOutputPageControl->Visible = true;

	// do LCx calculations
	Calc_LCx(LCxSD, LCxSD_low, LCxSD_high, LCX_T_END, gutsSD, error);
	if (error != "")
	{
		ShowMessage(error);
		return;
	}
	PlotLCx(CalibrationSDOutputLCxImage, gutsSD, LCX_T_END);
	Calc_LCx(LCxIT, LCxIT_low, LCxIT_high, LCX_T_END, gutsIT, error);
	if (error != "")
	{
		ShowMessage(error);
		return;
	}
	PlotLCx(CalibrationITOutputLCxImage, gutsIT, LCX_T_END);

	EnableGUIAfterCalibration();
	Screen->Cursor = crDefault;

	// Clear Batch projection results
	LPxBatch_Samplesize_Info_out.clear();
	LPxBatch_out.clear();
    BatchPredictionDlg->BatchMemo->Lines->Clear();

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::PlotLCx(TImage *img, int sdit, double x_max)
{
    Application->ProcessMessages();

	vector<vector<double> > lcx, lcx_low, lcx_high;

	if (sdit == gutsSD)
	{
		lcx = LCxSD;
		lcx_low = LCxSD_low;
		lcx_high = LCxSD_high;
	}
	else
	{
		lcx = LCxIT;
		lcx_low = LCxIT_low;
		lcx_high = LCxIT_high;
	}

    // setting up the chart
	TChart *chart = new TChart(this);
	chart->AllowPanning = Vcltee::Teeprocs::pmNone;
	chart->BevelOuter = bvNone;
	chart->Color = clWhite;
	chart->View3D = false;
	chart->View3DWalls = false;
	chart->Legend->Visible = true;
	chart->Legend->Shadow->Visible = false;

	chart->LeftAxis->Grid->Visible = false;
	chart->LeftAxis->Axis->Width = 1;
	chart->LeftAxis->AutomaticMinimum = false;
	chart->LeftAxis->Minimum = 0;
	chart->LeftAxis->AxisValuesFormat = "#,##0.0";
	chart->LeftAxis->Texts->Font->Color = clBlack;
	chart->LeftAxis->Title->Font->Size = 12;
	chart->LeftAxis->Title->Text = "LCx [" + ConcUnit + "]";

	chart->BottomAxis->Grid->Visible = false;
	chart->BottomAxis->Axis->Width = 1;
	chart->BottomAxis->AutomaticMinimum = false;
	chart->BottomAxis->Minimum = 0;
	chart->BottomAxis->AutomaticMaximum = false;
	chart->BottomAxis->Maximum = x_max;
	chart->BottomAxis->AxisValuesFormat = "#0";
	chart->BottomAxis->Texts->Font->Color = clBlack;
	chart->BottomAxis->Title->Font->Size = 12;
	chart->BottomAxis->Title->Text = "Time [days]";

	chart->Title->Text->Clear();


	// setting up the series
	chart->RemoveAllSeries();

	// LC50 plot
	// add area series
	chart->AddSeries(new TAreaSeries(chart));
	chart->Series[chart->SeriesCount() - 1]->Color = CL_CONF_BANDS_YELLOW;
	TAreaSeries *as1 = (TAreaSeries*)chart->Series[chart->SeriesCount() - 1];
	as1->AreaLinesPen->Visible = false;
	as1->LinePen->Style = psSolid;
	as1->LinePen->Color = clMedGray;
	chart->Series[chart->SeriesCount() - 1]->Legend->Text = "LC50";
	// add area series
	chart->AddSeries(new TAreaSeries(chart));
	chart->Series[chart->SeriesCount() - 1]->Color = clWhite;
	TAreaSeries *as2 = (TAreaSeries*)chart->Series[chart->SeriesCount() - 1];
	as2->AreaLinesPen->Visible = false;
	as2->LinePen->Style = psSolid;
	as2->LinePen->Color = clMedGray;
	chart->Series[chart->SeriesCount() - 1]->Legend->Visible = false;

	// LC10 plot
	// add area series
	chart->AddSeries(new TAreaSeries(chart));
	chart->Series[chart->SeriesCount() - 1]->Color = CL_CONF_BANDS_GREEN;
	TAreaSeries *as3 = (TAreaSeries*)chart->Series[chart->SeriesCount() - 1];
	as3->AreaLinesPen->Visible = false;
	as3->LinePen->Style = psSolid;
	as3->LinePen->Color = clMedGray;
	chart->Series[chart->SeriesCount() - 1]->Legend->Text = "LC10";
	// add area series
	chart->AddSeries(new TAreaSeries(chart));
	chart->Series[chart->SeriesCount() - 1]->Color = clWhite;
	TAreaSeries *as4 = (TAreaSeries*)chart->Series[chart->SeriesCount() - 1];
	as4->AreaLinesPen->Visible = false;
	as4->LinePen->Style = psSolid;
	as4->LinePen->Color = clMedGray;
	chart->Series[chart->SeriesCount() - 1]->Legend->Visible = false;

	// LC 50 plot
	// add line series
	chart->AddSeries(new TLineSeries(chart));
	chart->Series[chart->SeriesCount() - 1]->Color = clBlack;
	chart->Series[chart->SeriesCount() - 1]->Pen->Style = psSolid;
	chart->Series[chart->SeriesCount() - 1]->Legend->Visible = false;
	// add point series
	chart->AddSeries(new TPointSeries(chart));
	TPointSeries* ps1 = (TPointSeries*)chart->Series[chart->SeriesCount() - 1];
	ps1->Pointer->Style = psCircle;
	ps1->Pointer->Size = 3;
	ps1->Pointer->Color = CL_CONF_BANDS_YELLOW;
	ps1->Pointer->Pen->Color = clBlack;
	chart->Series[chart->SeriesCount() - 1]->Legend->Visible = false;

	// LC 10 plot
	// add line series
	chart->AddSeries(new TLineSeries(chart));
	chart->Series[chart->SeriesCount() - 1]->Color = clBlack;
	chart->Series[chart->SeriesCount() - 1]->Pen->Style = psSolid;
	chart->Series[chart->SeriesCount() - 1]->Legend->Visible = false;
	// add point series
	chart->AddSeries(new TPointSeries(chart));
	TPointSeries* ps2 = (TPointSeries*)chart->Series[chart->SeriesCount() - 1];
	ps2->Pointer->Style = psCircle;
	ps2->Pointer->Size = 3;
	ps2->Pointer->Color = CL_CONF_BANDS_GREEN;
	ps2->Pointer->Pen->Color = clBlack;
	chart->Series[chart->SeriesCount() - 1]->Legend->Visible = false;

	// plot LC50 and LC10
	for (int t=0; t<lcx.size(); t++)
	{
		// LC50 area series
		chart->Series[0]->AddXY(predT_LCX[t], lcx_high[t][0]);
		chart->Series[1]->AddXY(predT_LCX[t], lcx_low[t][0]);
		// LC10 area series
		chart->Series[2]->AddXY(predT_LCX[t], lcx_high[t][2]);
		chart->Series[3]->AddXY(predT_LCX[t], lcx_low[t][2]);

        // LC50 line and points
		chart->Series[4]->AddXY(predT_LCX[t], lcx[t][0]);
		chart->Series[5]->AddXY(predT_LCX[t], lcx[t][0]);
		// LC10 line and points
		chart->Series[6]->AddXY(predT_LCX[t], lcx[t][2]);
		chart->Series[7]->AddXY(predT_LCX[t], lcx[t][2]);
	}

	TRect rect;
	rect.Left = 0;
	rect.Top = 0;
	rect.Right = 1.5 * plotSINGLE_CHART_WIDTH;
	rect.Bottom = 1.0 * plotSINGLE_CHART_HEIGHT;
	img->Picture->Assign(chart->TeeCreateMetafile(true, rect));

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::PlotLPx(TImage *img, int sdit, bool paint_CI)
{
    Application->ProcessMessages();

	vector<vector<double> > lpx;

	if (sdit == gutsSD)
		lpx = LPxSD;
	else
		lpx = LPxIT;

	// setting up the chart
	TChart *chart = new TChart(this);
	chart->AllowPanning = Vcltee::Teeprocs::pmNone;
	chart->BevelOuter = bvNone;
	chart->Color = clWhite;
	chart->View3D = false;
	chart->View3DWalls = false;
	chart->Legend->Visible = false;

	chart->LeftAxis->Grid->Visible = false;
	chart->LeftAxis->Axis->Width = 1;
	chart->LeftAxis->AutomaticMinimum = false;
	chart->LeftAxis->Minimum = 0;
	chart->LeftAxis->AutomaticMaximum = false;
	chart->LeftAxis->Maximum = 1;
	chart->LeftAxis->AxisValuesFormat = "#,##0.0";
	chart->LeftAxis->Texts->Font->Color = clBlack;
	chart->LeftAxis->Title->Font->Size = 12;
	chart->LeftAxis->Title->Text = "Survival [0..1]";

	chart->BottomAxis->Grid->Visible = false;
	chart->BottomAxis->Axis->Width = 1;
	chart->BottomAxis->AxisValuesFormat = "#######0.0#";
	chart->BottomAxis->Texts->Font->Color = clBlack;
	chart->BottomAxis->Title->Font->Size = 12;
	chart->BottomAxis->Title->Text = "Multiplication factor LPx [-]";

	chart->Title->Text->Clear();


	// setting up the series
	chart->RemoveAllSeries();

	// LCx plot
	// add area series
	chart->AddSeries(new TAreaSeries(chart));
	chart->Series[0]->Color = CL_CONF_BANDS_GREEN;
	TAreaSeries *as1 = (TAreaSeries*)chart->Series[0];
	as1->AreaLinesPen->Visible = false;
	as1->LinePen->Style = psSolid;
	as1->LinePen->Color = clMedGray;
	// add area series
	chart->AddSeries(new TAreaSeries(chart));
	chart->Series[1]->Color = clWhite;
	TAreaSeries *as2 = (TAreaSeries*)chart->Series[1];
	as2->AreaLinesPen->Visible = false;
	as2->LinePen->Style = psSolid;
	as2->LinePen->Color = clMedGray;
	// add line series
	chart->AddSeries(new TLineSeries(chart));
	chart->Series[2]->Color = clBlack;
	chart->Series[2]->Pen->Style = psSolid;


	// plot LC50 (index 0)
	for (int mf=0; mf<lpx.size(); mf++)
	{
		if (paint_CI)
		{
			chart->Series[0]->AddXY(lpx[mf][0], lpx[mf][3]);
			chart->Series[1]->AddXY(lpx[mf][0], lpx[mf][2]);
		}
		chart->Series[2]->AddXY(lpx[mf][0], lpx[mf][1]);
	}

	int img_width = 1.25 * plotSINGLE_CHART_WIDTH;
	int img_height = 1.25 * plotSINGLE_CHART_HEIGHT;
	TRect rect;
	rect.Left = 0;
	rect.Top = 0;
	rect.Right = img_width;
	rect.Bottom = img_height;
	img->Width = img_width;
	img->Height = img_height;
	img->Picture->Assign(chart->TeeCreateMetafile(true, rect));

	return;
}
// ---------------------------------------------------------------------------

vector<double> TMainForm::GetMaximumTimePointsFromPreparedData(TPreparedDataContainer &data_all)
{
	Application->ProcessMessages();

	vector<double>group_t_max;
	vector<double>data_set_t_max;
	for (int d = 0; d < data_all.Concentration.size(); d++)
	{
		group_t_max.resize(0);
		for (int g = 0; g < data_all.Survival[d].size(); g++)
			group_t_max.push_back(data_all.Survival[d][g].Times[data_all.Survival[d][g].Times.size() - 1]);
		data_set_t_max.push_back(MaxNumInVec(group_t_max));
	}

	return data_set_t_max;
}
// ---------------------------------------------------------------------------

void TMainForm::DetermineMaximumSurvivalTime(const vector<TPreparedTreatmentSurv> &ev_ic)
{
	double _maxTimeOverAllTreatments = 0.0;
	std::vector<double> _v;
	std::vector<double>::iterator _result;

	// iterate over all treatment groups
	for (int g = 0; g < ev_ic.size(); ++g)
	{
		// within the treatment group, determine last 'Times' element
		int _numTimes = ev_ic[g].Times.size();
		// note down the latest time entry - in the vector for later determination of the maximum
		_v.push_back(ev_ic[g].Times[_numTimes - 1]);
	}

	_result = std::max_element(_v.begin(), _v.end());
	_maxTimeOverAllTreatments = *_result;
	// Save it in global variable for later use
	MaximumSurvivalTime = _maxTimeOverAllTreatments;
}


void TMainForm::Calc_Conf_Per_Data_Set(vector<TSimulationOutput> &data_set_out, vector<double> &sim_time,
	vector<TPreparedTreatmentConc> &ev_ic, int sdit, double &hb_fix, int flag_conf, AnsiString &err)
{
	data_set_out.resize(0);

	for (int g = 0; g < ev_ic.size(); g++)
	{
		Application->ProcessMessages();
		TSimulationOutput treatment_out;

		Calc_Conf(treatment_out, sim_time, ev_ic[g], sdit, hb_fix, flag_conf, err);
		if (err != "")
			return;
		data_set_out.push_back(treatment_out);
	}

	return;
}
// ---------------------------------------------------------------------------

bool TMainForm::DoSimulation(vector<double> &output_time, TPreparedTreatmentConc &prep_conc, vector<double> &params,
	TMetafile *metafile, int paint, int sdit, AnsiString &err)
{
	Application->ProcessMessages();

	if (sdit == gutsSDIT_BOTH)
	{
		DoSimulation(output_time, prep_conc, params, metafile, paint, gutsSD, err);
		if (err != "")
			return false;
		DoSimulation(output_time, prep_conc, params, metafile, paint, gutsIT, err);
		if (err != "")
			return false;
		return true;
	}

	Calc_Model(output_time, params, prep_conc, sdit, err);
	if (err != "")
		return false;
	if (StopBatchPredictions)
	{
		return true;
	}

	if (paint == plotDO_PAINT)
	{
		// paint
	}

	return true;
}
// ---------------------------------------------------------------------------

bool TMainForm::CheckGetAndPreparePredictionsData(TStringGrid *grid)
{
    Application->ProcessMessages();

	// check data
	PredictionsDataChecked = false;
	if (!PredictionsDataChecked)
		if (!CheckGridData(grid, gutsCONC_DATA))
			return false;
	PredictionsDataChecked = true;

	// get data from grid
	GetDataFromGrid(grid, SingleExposureProfile);

	// build data structure
	PredictionsData.resize(1);
	PredictionsData[0].Concentration = SingleExposureProfile;

	// add dummy survival data as proposed by Tjalling Jager
	PredictionsData[0].Survival.Times.resize(2);
	PredictionsData[0].Survival.Times[0] = 0;
	PredictionsData[0].Survival.Times[1] = SingleExposureProfile.Times[SingleExposureProfile.Times.size() - 1];

	// prepare data
	Prepare_Data(PredictionsData_All, PredictionsData_HB, PredictionsData, PredictionsData.size(), false);

	return true;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::StartSimulationButtonClick(TObject *Sender)
{
	Application->ProcessMessages();

	if (!CalibrationPerformed)
	{
		ShowMessage("A prediction can only be performed after a successful calibration of the (unchanged) input data sets!");
		return;
	}

	Screen->Cursor = crHourGlass;
	DisableGUIForPredictions();

	// CHECK, GET and PREPARE the predictions data
	if (!CheckGetAndPreparePredictionsData(PredictionsConcStringGrid))
	{
		EnableGUIAfterPredictions();
		Screen->Cursor = crDefault;
		return;
	}

	// prepare simulation
	TScrollBox *scrollbox;
	TMemo *memo;
	int sdit;
	vector<TParameterSpace> ParaSample;
	if (PredictionsGutsSDCheckBox->Checked)
	{
		scrollbox = PredictionsSDSurvivalScrollBox;
		memo = PredictionsSDOutputMemo;
		sdit = gutsSD;
		PredictionsSDOutputPageControl->Visible = false;
		ParaSample = CalibrationParameterSpaceSD;
	}
	else
	{
		scrollbox = PredictionsITSurvivalScrollBox;
		memo = PredictionsITOutputMemo;
		sdit = gutsIT;
		PredictionsITOutputPageControl->Visible = false;
		ParaSample = CalibrationParameterSpaceIT;
	}
	memo->Clear();
	PredictionsProgressBar->Position = 0;

	// CALCULATE LPx
	vector<vector<double> >LPx, LP_low, LP_high;
	vector<AnsiString>memo_strings;
	int flag;
	if (LPxCalcCICheckBox->Checked)
	{
		flag = 1; 	// regular LPx calulation with calculation of CI
		ParaSample = ParameterSpaceSubsetForErrorPropagation(ParaSample);
	}
	else
	{
		flag = 2;   // no CI (very fast)
        ParaSample.resize(0);
	}
	AnsiString err = "";
	Calc_LPx(LPx, LP_low, LP_high,
		PredictionsData_All.Concentration[0], flag, sdit, PREDICTION,
		memo, memo_strings, err,
		ParaSample
	);
	if (err != "")
	{
		ShowMessage(err);
		return;
	}


	if (AbortPredictions)
	{
		PredictionsSDOutputMemo->Clear();
		PredictionsSDOutputMemo->Lines->Add("Predictions aborted!");
		PredictionsITOutputMemo->Clear();
		PredictionsITOutputMemo->Lines->Add("Predictions aborted!");
		EnableGUIAfterPredictions();
		Screen->Cursor = crDefault;
		AbortPredictions = false;
		return;
	}


	// ADD LPx DATA to PredictionsData_All
	PredictionsData_All.Concentration[0].resize(LPx[0].size() + 1);
	PredictionsData_All.GroupNames[0].resize(LPx[0].size() + 1);
	PredictionsData_All.GroupNames[0][0] = "MF = 1";
	for (int g = 1; g < PredictionsData_All.Concentration[0].size(); g++)
	{
		PredictionsData_All.Concentration[0][g] = PredictionsData_All.Concentration[0][0];
		PredictionsData_All.GroupNames[0][g] = "MF = " + MyFloatToStr(LPx[0][g - 1], 4);
		for (int t = 0; t < PredictionsData_All.Concentration[0][g].Times.size(); t++)
		{
			PredictionsData_All.Concentration[0][g].Conc[t] *= LPx[0][g - 1];
			PredictionsData_All.Concentration[0][g].Slope[t] *= LPx[0][g - 1];
		}
	}

	// PAINTING
	double hb_fix = -1;
	TImage *dummy_img;
	PaintGUTSGraphs(PredictionsData_All, 1, hb_fix, scrollbox, dummy_img, PREDICTION, sdit, err);
	if (err != "")
	{
		ShowMessage(err);
		return;
	}
	if (sdit == gutsSD)
		PlotLPx(PredictionsSDOutputMultiplicationFactorImage, sdit, LPxCalcCICheckBox->Checked);
	else
		PlotLPx(PredictionsITOutputMultiplicationFactorImage, sdit, LPxCalcCICheckBox->Checked);


	PredictionsPageControl->ActivePage = PredictionsOutputTab;
	if (sdit == gutsSD)
	{
		SDPredictionsPerformed = true;
		PredictionsSDOutputPageControl->Visible = true;
		PredictionsOutputPageControl->ActivePage = PredictionsSDOutputTab;
		PredictionsSDOutputPageControl->ActivePage = PredictionsSDOutputSurvivalTab;
	}
	else
	{
		ITPredictionsPerformed = true;
		PredictionsITOutputPageControl->Visible = true;
		PredictionsOutputPageControl->ActivePage = PredictionsITOutputTab;
		PredictionsITOutputPageControl->ActivePage = PredictionsITOutputSurvivalTab;
	}

	EnableGUIAfterPredictions();
	Screen->Cursor = crDefault;

	return;
}
// ---------------------------------------------------------------------------

AnsiString TMainForm::GetDataSetNumber(AnsiString grid_name)
{
	// extract the data set number from the grid name
	AnsiString as = "";
	AnsiString sub_as;
	for (int i = 1; i <= grid_name.Length(); i++)
	{
		sub_as = grid_name.SubString(i, 1);
		if (StringIsInteger(sub_as.c_str()))
			as += sub_as;
	}

	return as;
}
// ---------------------------------------------------------------------------

bool TMainForm::CheckGridData(TStringGrid *grid, int data_type)
{
    Application->ProcessMessages();

	int number_of_groups = -1;
	int number_of_times = -1;
	double current_time_point, last_time_point;
	int current_value, last_value;
	string s;
	vector<string>sv;

	AnsiString data_set_num = GetDataSetNumber(AnsiString(grid->Name));

	// check column and row headers defining the dimension of the grid data
	for (int c = grid->ColCount - 1; c >= 0; c--)
	{
		if (number_of_groups < 0 && grid->Cells[c][0] != "")
			number_of_groups = c;
		// not +1 because of time column at the beginning
		else if (number_of_groups >= 0 && grid->Cells[c][0] == "")
		{
			ShowMessage("Error in " +
				AnsiString(data_type==gutsCONC_DATA ? "conc. grid of" : "survival grid of") +
				" data set " + data_set_num +
				".\nPlease avoid empty column headers within data area in column no. " + AnsiString(c + 1) + ".");
			return false;
		}
		else
			continue;
	}
	for (int r = grid->RowCount - 1; r > 0; r--) // only iterate to r>0, r==0 is already done above
	{
		if (grid->Cells[0][r] != "") // filled cell ...
		{
			if (grid->Cells[0][r] == MISSING_DATA_SIGN.c_str())
				// ... but missing data in first column
			{
				ShowMessage("Error in " +
					AnsiString(data_type==gutsCONC_DATA ? "conc. grid of" : "survival grid of") +
					" data set " + data_set_num +
					".\nFirst column is reserved for time vector. Only enter valid positive floating-point numbers in ascending order.\nError in row no. " +
					AnsiString(r + 1) + ".");
				return false;
			}
			if (!StringIsDouble(AnsiString(grid->Cells[0][r]).c_str()))
				// ... but not a double
			{
				ShowMessage("Error in " +
					AnsiString(data_type==gutsCONC_DATA ? "conc. grid of" : "survival grid of") +
					" data set " + data_set_num +
					".\nFirst column is reserved for time vector. Only enter valid positive floating-point numbers in ascending order.\nError in row no. " +
					AnsiString(r + 1) + ".");
				return false;
			}
			current_time_point = grid->Cells[0][r].ToDouble();
			if (data_type == gutsCONC_DATA)
				if (number_of_times >= 0 && last_time_point < current_time_point) // ... but not in correct order
				{
					ShowMessage("Error in " +
						AnsiString(data_type==gutsCONC_DATA ? "conc. grid of" : "survival grid of") +
						" data set " + data_set_num +
						".\nFirst column is reserved for time vector. Only enter valid positive floating-point numbers in ascending order.\nError in row no. " +
						AnsiString(r + 1) + ".");
					return false;
				}
			if (data_type == gutsSURV_DATA)
				if (number_of_times >= 0 && last_time_point <= current_time_point)
					// ... but not in correct order (time point must be unique)
				{
					ShowMessage("Error in " +
						AnsiString(data_type==gutsCONC_DATA ? "conc. grid of" : "survival grid of") +
						" data set " + data_set_num +
						".\nFirst column is reserved for time vector. Only enter valid positive floating-point numbers in ascending order.\nError in row no. " +
						AnsiString(r + 1) + ".");
					return false;
				}
			last_time_point = current_time_point;
			if (number_of_times < 0)
				number_of_times = r;
			// get size of time vector; not +1 because of header row
		}
		else if (number_of_times >= 0 && grid->Cells[0][r] == "")
		{
			ShowMessage("Error in " +
				AnsiString(data_type==gutsCONC_DATA ? "conc. grid of" : "survival grid of") +
				" data set " + data_set_num + ".\nAvoid empty row headers within data area in row no. " +
				AnsiString(r + 1) + ".");
			return false;
		}
		else
			continue;
	}
	if (number_of_groups < 1)
	{
		ShowMessage("Error in " +
			AnsiString(data_type==gutsCONC_DATA ? "conc. grid of" : "survival grid of") +
			" data set " + data_set_num + ".\nEnter at least one treatment group.");
		return false;
	}
	if (data_type==gutsCONC_DATA && number_of_times < 1)
	{
		ShowMessage("Error in " +
			AnsiString(data_type==gutsCONC_DATA ? "conc. grid of" : "survival grid of") +
			" data set " + data_set_num + ".\nEnter at least one time event.");
		return false;
	}
	if (data_type==gutsSURV_DATA && number_of_times < 2)
	{
		ShowMessage("Error in " +
			AnsiString(data_type==gutsCONC_DATA ? "conc. grid of" : "survival grid of") +
			" data set " + data_set_num + ".\nEnter at least two time events.");
		return false;
	}

	// check if there is any grid data out of the defined data dimensions
	for (int c = number_of_groups + 1; c < grid->ColCount; c++)
		for (int r = 0; r < grid->RowCount; r++)
			if (grid->Cells[c][r] != "")
			{
				ShowMessage("Error in " +
					AnsiString(data_type==gutsCONC_DATA ? "conc. grid of" : "survival grid of") +
					" data set " + data_set_num +
					".\nArea out of labelled columns and rows may not contain data!");
				return false;
			}
	for (int c = 0; c < number_of_groups + 1; c++)
		for (int r = number_of_times + 1; r < grid->RowCount; r++)
			if (grid->Cells[c][r] != "")
			{
				ShowMessage("Error in " +
					AnsiString(data_type==gutsCONC_DATA ? "conc. grid of" : "survival grid of") +
					" data set " + data_set_num +
					".\nArea out of labelled columns and rows may not contain data!");
				return false;
			}

	// check availability of time units in grid
	s = AnsiString(grid->Cells[0][0]).c_str();
	sv = SplitString(s, "[");
	if (sv.size() < 2)
	{
		ShowMessage("Error in " +
			AnsiString(data_type==gutsCONC_DATA ? "conc. grid of" : "survival grid of") +
			" data set " + data_set_num +
			".\nMissing time unit in grids. Append the time unit in squared brackets [...] to the time column header.");
		return false;
	}
	sv = SplitString(sv[sv.size() - 1], "]");
	if (sv.size() < 2)
	{
		ShowMessage("Error in " +
			AnsiString(data_type==gutsCONC_DATA ? "conc. grid of" : "survival grid of") +
			"data set " + data_set_num +
			".\nMissing time unit in grids. Append the time unit in squared brackets [...] to the time column header.");
		return false;
	}

	// check data area for adequacy
	if (grid->Cells[0][1].ToDouble() != 0)
		// first element of time vector must be zero
	{
		ShowMessage("Error in " +
			AnsiString(data_type==gutsCONC_DATA ? "conc. grid of" : "survival grid of") +
			" data set " + data_set_num + ".\nFirst element of time vector in first column must be zero!");
		return false;
	}
	vector<int>values; // vector for check of decreasing order of survival data
	for (int c = 1; c < number_of_groups + 1; c++) // +1 because of header
	{
		values.resize(0);
		// do not allow missing or invalid data in first data row
		if (grid->Cells[c][1] == "" || grid->Cells[c][1] == MISSING_DATA_SIGN.c_str())
		{
			ShowMessage("Error in " +
				AnsiString(data_type==gutsCONC_DATA ? "conc. grid of" : "survival grid of") +
				" data set " + data_set_num + ".\nMissing data not allowed in the first data row of each column");
			return false;
		}
		for (int r = 1; r < number_of_times + 1; r++) // +1 because of header
		{
			if (AnsiString(grid->Cells[c][r]) == "-" || grid->Cells[c][r] == "NAN" || grid->Cells[c][r] == "NaN" ||
					grid->Cells[c][r] == "nan" || grid->Cells[c][r] == "N/A" || grid->Cells[c][r] == "n/a" ||
					(AnsiString(grid->Cells[c][r]) != "" && Trim(grid->Cells[c][r]) == ""))
				grid->Cells[c][r] = "";
			if (grid->Cells[c][r] != "") // if cell is filled
			{
				if (StringIsDouble(AnsiString(grid->Cells[c][r]).c_str())
					&& grid->Cells[c][r].ToDouble() < 0)
					// if there are negative values
				{
					ShowMessage("Negative value in " +
							AnsiString(data_type==gutsCONC_DATA ? "conc. grid of" : "survival grid of") +
							" data set " + data_set_num +
							".\nEnter valid positive floating-point number or missing data representation '-' in [row " +
							AnsiString(r + 1) + ", col " + AnsiString(c + 1) +
							"]. Floating-point numbers can e.g. be entered as 0.0842 or 8.42E-02");
					return false;
				}
				if (data_type == gutsCONC_DATA && !StringIsDouble(AnsiString(grid->Cells[c][r]).c_str())
					&& grid->Cells[c][r] != MISSING_DATA_SIGN.c_str())
					// if cell does not contain floating-point number or defined missing data
				{
					ShowMessage("Error in " +
						AnsiString(data_type==gutsCONC_DATA ? "conc. grid of" : "survival grid of") +
						" data set " + data_set_num +
						".\nEnter valid positive floating-point number or missing data representation '-' in [row " +
						AnsiString(r + 1) + ", col " + AnsiString(c + 1) +
						"]. Floating-point numbers can e.g. be entered as 0.0842 or 8.42E-02");
					return false;
				}
				if (data_type == gutsSURV_DATA && !StringIsInteger(AnsiString(grid->Cells[c][r]).c_str())
					&& grid->Cells[c][r] != MISSING_DATA_SIGN.c_str())
					// if cell does not contain floating-point number or defined missing data
				{
					ShowMessage("Error in " +
						AnsiString(data_type==gutsCONC_DATA ? "conc. grid of" : "survival grid of") +
						" data set " + data_set_num +
						".\nEnter valid positive integer or missing data representation '-' in [row " + AnsiString(r + 1) +
						", col " + AnsiString(c + 1) + "].");
					return false;
				}
				if (data_type == gutsSURV_DATA && grid->Cells[c][r] != MISSING_DATA_SIGN.c_str())
				{
					values.push_back(grid->Cells[c][r].ToInt());
				}
				if (data_type == gutsSURV_DATA && values.size() > 1 && values[values.size() - 1] > values[values.size() - 2])
				{
					ShowMessage("Error in data set " + data_set_num + ".\nAscending survival not allowed. Error in column '" +
						AnsiString(grid->Cells[c][0]) + "'.");
					return false;
				}
			}
		}
	}

	return true;
}
// ---------------------------------------------------------------------------

bool TMainForm::StringIsDouble(string s)
{
    Application->ProcessMessages();

	// check if string s is a floating-point number or missing data or empty
	double d;
	char* end;
	d = std::strtod(s.c_str(), &end);
	string end_s = end;
	if (end_s.size() == 0 || s == MISSING_DATA_SIGN || s == "")
		// if double OR missing data OR empty
			return true;
	return false;
}
// ---------------------------------------------------------------------------

bool TMainForm::StringIsInteger(string s)
{
    Application->ProcessMessages();

	// check if string s is an integer or missing data
	int i;
	char* end;
	i = std::strtol(s.c_str(), &end, 10);
	string end_s = end;
	if (end_s.size() == 0 || s == MISSING_DATA_SIGN || s == "")
		// if integer OR missing data OR empty
			return true;
	return false;
}
// ---------------------------------------------------------------------------

bool TMainForm::CheckConcSurvCompatibility(TStringGrid *conc_grid, TStringGrid *surv_grid)
	// checks if group names are identical in survival and conc grids
{
	// check concentration grid and survival grid for compatibility by comparing headers
	int min_cols = Min(conc_grid->ColCount, surv_grid->ColCount);
	for (int c = 1; c < min_cols; c++) // from i=1 --> header of time vector irrelevant
	{
		if (conc_grid->Cells[c][0] != surv_grid->Cells[c][0])
		{
			ShowMessage(
				"The concentration grid headers have to match the survival grid headers column by column (except time header in column no. 1)!");
			return false;
		}
	}
	for (int c = min_cols; c < conc_grid->ColCount; c++) // checking for more data beyond the dimension of the corresponding grid
	{
		if (conc_grid->Cells[c][0] != "")
		{
			ShowMessage(
				"The concentration grid headers have to match the survival grid headers column by column (except time header in column no. 1)!");
			return false;
		}
	}
	for (int c = min_cols; c < surv_grid->ColCount; c++) // checking for more data beyond the dimension of the corresponding grid
	{
		if (surv_grid->Cells[c][0] != "")
		{
			ShowMessage(
				"The concentration grid headers have to match the survival grid headers column by column (except time header in column no. 1)!");
			return false;
		}
	}

	return true;
}
// ---------------------------------------------------------------------------

bool TMainForm::CheckCalibControlGroupAvailability()
	// checking the availabilty of control groups (at least one)
{
	for (int i = 0; i < NumberOfCalibrationDataSets; i++)
	{
		if (CalibControlCombos[i]->ItemIndex < CalibControlCombos[i]->Items->Count - 1)
			return true;
	}

	ShowMessage("Define at least one control group!");

	return false;
}
// ---------------------------------------------------------------------------

bool TMainForm::CheckValidControlGroupAvailability()
	// checking the availabilty of control groups (at least one)
{
	if (ValidControlCombo->ItemIndex < ValidControlCombo->Items->Count - 1)
		return true;

	ShowMessage("Please define a control group!");

	return false;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::StartValidationButtonClick(TObject *Sender)
	// do a validation for the calibrated parameters
{
	Application->ProcessMessages();

	if (!CalibrationPerformed)
	{
		ShowMessage("A validation can only be performed after a successful calibration of the (unchanged) input data sets!");
		return;
	}

	Screen->Cursor = crHourGlass;
	SDValidationPerformed = false;
	ITValidationPerformed = false;

	ValidationSDOutputMemo->Clear();
	ValidationITOutputMemo->Clear();
	ValidationSDOutputPageControl->Visible = false;
	ValidationITOutputPageControl->Visible = false;

	DisableGUI();

	// CHECK and GET validation data
	if (!CheckAndGetValidationData(ValidationData))
	{
		EnableGUI();
		Screen->Cursor = crDefault;
		return;
	}

	ValidationTab->Enabled = false;

	// prepare the validation data set (formatting to suitable structure)
	Prepare_Data(ValidationData_All, ValidationData_HB, ValidationData, ValidationData.size(), FitHBToValidationControl);

	double hb_fix;
	if (FitHBToValidationControl)
		hb_fix = ValidationData_All.BackgroundHazard;
	else
		hb_fix = -1.0; // identifyer (don't fix background mortality in sample)

	// validate
	TImage *dummy_img;

	AnsiString err;
	// SD
	PaintGUTSGraphs(ValidationData_All, 1, hb_fix, ValidationSDOutputSurvivalScrollBox, dummy_img, VALIDATION, gutsSD, err);
	if (err != "")
	{
		ShowMessage(err);
		return;
	}
	// write GOODNESS OF FIT to memo
	DisplayGoodnessOfFit(ValidationData_All, ValidationSDOutputMemo, gutsSD, VALIDATION);

	// IT
	PaintGUTSGraphs(ValidationData_All, 1, hb_fix, ValidationITOutputSurvivalScrollBox, dummy_img, VALIDATION, gutsIT, err);
	if (err != "")
	{
		ShowMessage(err);
		return;
	}
	// write GOODNESS OF FIT to memo
	DisplayGoodnessOfFit(ValidationData_All, ValidationITOutputMemo, gutsIT, VALIDATION);

	ValidationResultsPlotted = true;

	ValidationSDOutputTab->Visible = true;
	ValidationITOutputTab->Visible = true;

	ValidationPageControl->ActivePage = ValidationOutputTab;

	// SD
	ValidationSDOutputPageControl->Visible = true;
	ValidationOutputPageControl->ActivePage = ValidationSDOutputTab;
	ValidationSDOutputPageControl->ActivePage = ValidationSDOutputSurvivalTab;
	SDValidationPerformed = true;

	// IT
	ValidationITOutputPageControl->Visible = true;
	ValidationOutputPageControl->ActivePage = ValidationITOutputTab;
	ValidationITOutputPageControl->ActivePage = ValidationITOutputSurvivalTab;
	ITValidationPerformed = true;

	EnableGUI();
	Screen->Cursor = crDefault;

	return;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::PredictionsGutsSDCheckBoxClick(TObject *Sender)
{
	if (PredictionsGutsSDCheckBox->Checked)
	{
		PredictionsSDOutputTab->TabVisible = true;
		PredictionsGutsITCheckBox->Checked = false;
		PredictionsITOutputTab->TabVisible = false;
	}
	else
	{
		PredictionsSDOutputTab->TabVisible = false;
		PredictionsGutsITCheckBox->Checked = true;
		// choose IT if SD is disabled
	}

	return;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::PredictionsGutsITCheckBoxClick(TObject *Sender)
{
	if (PredictionsGutsITCheckBox->Checked)
	{
		PredictionsITOutputTab->TabVisible = true;
		PredictionsGutsSDCheckBox->Checked = false;
		PredictionsSDOutputTab->TabVisible = false;
	}
	else
	{
		PredictionsITOutputTab->TabVisible = false;
		PredictionsGutsSDCheckBox->Checked = true;
		// choose SD if IT is disabled
	}

	return;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::InputDataCalibSetNoSpinChange(TObject *Sender)
{
	for (int i = 0; i < MAX_CALIBRATION_DATA_SETS_NO; i++)
	{
		if (i < InputDataCalibSetNoSpin->Value)
			CalibrationDataSetTabs[i]->TabVisible = true;
		else
			CalibrationDataSetTabs[i]->TabVisible = false;
	}
	NumberOfCalibrationDataSets = InputDataCalibSetNoSpin->Value;

	ToDoAfterInputDataManipulation();

	return;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::PredictionsUseExposureProfileRadioButtonClick(TObject *Sender)

{
	// enable all exposure profile elements
	PredDataOpenSpeed->Enabled = true;
	PredDataSaveSpeed->Enabled = true;
	PredictionsConcStringGrid->Enabled = true;

	return;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::PredictionsUseBatchProcessingRadioButtonClick(TObject *Sender)

{
	// disable all exposure profile elements
	PredDataOpenSpeed->Enabled = false;
	PredDataSaveSpeed->Enabled = false;
	PredictionsConcStringGrid->Enabled = false;

	return;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::DisplayValidationDataButtonClick(TObject *Sender)
{
	// first CHECK and GET the calibration input data
	if (!CheckAndGetValidationData(TestValidationData))
	{
		return;
	}

	Screen->Cursor = crHourGlass;

	PaintInputData(ValidationInspectDataImage, VALIDATION);

	ValidationInspectDataTab->Visible = true;
	ValidInspectDataScrollBox->Visible = true;
	ValidationPageControl->ActivePage = ValidationInspectDataTab;

	Screen->Cursor = crDefault;

	return;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::DisplayPredictionsDataButtonClick(TObject *Sender)
{
	Screen->Cursor = crHourGlass;

	// first CHECK then GET the prediction input data
	if (!CheckGridData(PredictionsConcStringGrid, gutsCONC_DATA))
	{
		Screen->Cursor = crDefault;
		return;
	}
	PredictionsDataChecked = true;

	GetDataFromGrid(PredictionsConcStringGrid, SingleExposureProfile);

	// setup prediction data
	TestPredictionsData[0].Concentration = SingleExposureProfile;
	// dummy survival data as proposed by Tjalling Jager
	TestPredictionsData[0].Survival.Times[1] = SingleExposureProfile.Times[SingleExposureProfile.Times.size() - 1];

	PaintInputData(PredictionsInspectDataImage, PREDICTION);

	PredictionsInspectDataTab->Visible = true;
	PredInspectDataScrollBox->Visible = true;
	PredictionsPageControl->ActivePage = PredictionsInspectDataTab;

	Screen->Cursor = crDefault;

	return;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::CopyDataToClipboardClick(TObject *Sender)
{
	Screen->Cursor = crHourGlass;
	Clipboard()->SetTextBuf(Image->Hint.c_str());
	Screen->Cursor = crDefault;

	return;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::CopyImageToClipboardClick(TObject *Sender)
{
	Screen->Cursor = crHourGlass;
	Clipboard()->Assign(Image->Picture);
	Screen->Cursor = crDefault;

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::ParameterLikelihoodToChartDataPoints(vector<TChartDataPoint> &plot_data, vector<TParameterSpace> &coll,
	int parameter_1)
	// create chart data points (X, Y) from one parameter of a parameter space object and its likelihood
{
	Application->ProcessMessages();

	plot_data.resize(coll.size());
	for (int i = 0; i < plot_data.size(); i++)
	{
		plot_data[i].X = coll[i].Parameter[parameter_1];
		plot_data[i].Y = coll[i].Likelihood;
	}

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::ParameterSpaceToChartDataPoints(vector<TChartDataPoint> &plot_data, vector<TParameterSpace> &coll,
	int parameter_1, int parameter_2)
	// create chart data points (X, Y) from two parameters of a parameter space object
{
	Application->ProcessMessages();

	plot_data.resize(coll.size());
	for (int i = 0; i < plot_data.size(); i++)
	{
		plot_data[i].X = coll[i].Parameter[parameter_1];
		plot_data[i].Y = coll[i].Parameter[parameter_2];
	}

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::PaintParameterSpacePlot()
	// painting the parameter space plot (panel plot of parameter space in the
	// lower triangle matrix and likelihood profiles on the main diagonal)
{
	Application->ProcessMessages();

	for (int sdit = gutsSD; sdit <= gutsIT; sdit++)
	{
		TImage *img;
		TParameterMatrix pmat;
		vector<vector<TParameterSpace> >profile_likelihood;
		if (sdit == gutsSD)
		{
			img = CalibrationSDOutputParamImage;
			pmat = PMatSD;
			profile_likelihood = CalibrationProfileLikelihoodsSD;
		}
		else
		{
			img = CalibrationITOutputParamImage;
			pmat = PMatIT;
			profile_likelihood = CalibrationProfileLikelihoodsIT;
		}
		vector<int>ind_fit; // index to fitted parameters
		vector<AnsiString>parameter_labels;
		for (int p = 0; p < gutsPARAMETER_NO; p++)
			if (pmat.Fit[p] == gutsFIT_PARAM)
			{
				ind_fit.push_back(p);
				if (pmat.Scale[p] == gutsLOG_SCALE_PARAM)
					parameter_labels.push_back(AnsiString(gutsPARAMETERS[p].c_str()) + " (log)");
				else
					parameter_labels.push_back(AnsiString(gutsPARAMETERS[p].c_str()));
			}

		vector<AnsiString>empty_labels;
		empty_labels.resize(parameter_labels.size());
		fill(empty_labels.begin(), empty_labels.end(), " ");

		// clear existing graph and set size
		img->Picture->Metafile->Clear();
		img->Width = ind_fit.size() * plotSINGLE_CHART_WIDTH;
		img->Height = ind_fit.size() * plotSINGLE_CHART_HEIGHT;

		TMetafile *param_space_metafile = new TMetafile();
		param_space_metafile->SetSize(img->Width, img->Height);
		TMetafileCanvas* psmf_canvas = new TMetafileCanvas(param_space_metafile, 0);

		// prepare the data for the single parameter space plots
		vector<TParameterSpace>best_value, inner_rim, outer_rim;
		// data objects to plot
		vector<double> param_min, param_max; // min, max for axis plotting
        AnsiString error;
		PrepareDataForParameterSpacePlot(best_value, inner_rim, outer_rim, param_min, param_max, sdit, error);
		if (error != "")
		{
			ShowMessage(error);
			return;
        }

		if (ind_fit.size() > 1)
		{
			for (int p = 0; p < ind_fit.size() - 1; p++)
			{
				for (int q = p + 1; q < ind_fit.size(); q++)
				{
					vector<TChartDataPoint>plot_data_best_value;
					vector<TChartDataPoint>plot_data_inner_rim;
					vector<TChartDataPoint>plot_data_outer_rim;

					ParameterSpaceToChartDataPoints(plot_data_best_value, best_value, ind_fit[p], ind_fit[q]);
					ParameterSpaceToChartDataPoints(plot_data_inner_rim, inner_rim, ind_fit[p], ind_fit[q]);
					ParameterSpaceToChartDataPoints(plot_data_outer_rim, outer_rim, ind_fit[p], ind_fit[q]);

					TMetafile *metafile = new TMetafile();
					if (q == ind_fit.size() - 1 && p == 0)
						PaintSingleParameterSpacePlot(metafile, plot_data_best_value, plot_data_inner_rim, plot_data_outer_rim,
						param_min[ind_fit[p]], param_max[ind_fit[p]], param_min[ind_fit[q]], param_max[ind_fit[q]],
						parameter_labels[p], parameter_labels[q]);
					else if (q == ind_fit.size() - 1)
						PaintSingleParameterSpacePlot(metafile, plot_data_best_value, plot_data_inner_rim, plot_data_outer_rim,
						param_min[ind_fit[p]], param_max[ind_fit[p]], param_min[ind_fit[q]], param_max[ind_fit[q]],
						parameter_labels[p], " ");
					else if (p == 0)
						PaintSingleParameterSpacePlot(metafile, plot_data_best_value, plot_data_inner_rim, plot_data_outer_rim,
						param_min[ind_fit[p]], param_max[ind_fit[p]], param_min[ind_fit[q]], param_max[ind_fit[q]], " ",
						parameter_labels[q]);
					else
						PaintSingleParameterSpacePlot(metafile, plot_data_best_value, plot_data_inner_rim, plot_data_outer_rim,
						param_min[ind_fit[p]], param_max[ind_fit[p]], param_min[ind_fit[q]], param_max[ind_fit[q]], " ", " ");
					psmf_canvas->Draw(p*plotSINGLE_CHART_WIDTH, q*plotSINGLE_CHART_HEIGHT, metafile);
					delete metafile;
				}
			}
			for (int p = 0; p < ind_fit.size(); p++)
			{
				vector<TChartDataPoint>plot_data_best_value;
				vector<TChartDataPoint>plot_data_inner_rim;
				vector<TChartDataPoint>plot_data_outer_rim;
				vector<TChartDataPoint>plot_data_profile_likelihood;

				ParameterLikelihoodToChartDataPoints(plot_data_best_value, best_value, ind_fit[p]);
				ParameterLikelihoodToChartDataPoints(plot_data_inner_rim, inner_rim, ind_fit[p]);
				ParameterLikelihoodToChartDataPoints(plot_data_outer_rim, outer_rim, ind_fit[p]);
				if (profile_likelihood.size() > 0)  // if data is loaded from project file, the size is 0
					ParameterLikelihoodToChartDataPoints(plot_data_profile_likelihood, profile_likelihood[ind_fit[p]], ind_fit[p]);

				TMetafile *metafile = new TMetafile();
				if (p == 0)
					PaintSingleLikelihoodProfilePlot(metafile, plot_data_best_value, plot_data_inner_rim, plot_data_outer_rim,
					plot_data_profile_likelihood, param_min[ind_fit[p]], param_max[ind_fit[p]], " ", "loglik ratio");
				else if (p == ind_fit.size() - 1)
					PaintSingleLikelihoodProfilePlot(metafile, plot_data_best_value, plot_data_inner_rim, plot_data_outer_rim,
					plot_data_profile_likelihood, param_min[ind_fit[p]], param_max[ind_fit[p]], parameter_labels[p], " ");
				else
					PaintSingleLikelihoodProfilePlot(metafile, plot_data_best_value, plot_data_inner_rim, plot_data_outer_rim,
					plot_data_profile_likelihood, param_min[ind_fit[p]], param_max[ind_fit[p]], " ", " ");
				psmf_canvas->Draw(p*plotSINGLE_CHART_WIDTH, p*plotSINGLE_CHART_HEIGHT, metafile);
				delete metafile;
			}

			// paint legend
			TMetafile *metafile = new TMetafile();
			PaintParameterSpacePlotLegend(metafile);
			psmf_canvas->Draw((ind_fit.size() - 1)*plotSINGLE_CHART_WIDTH, 0, metafile);
			delete metafile;
		}

		delete psmf_canvas;

		TBitmap *param_space_bitmap = new TBitmap;
		param_space_bitmap->SetSize(param_space_metafile->Width, param_space_metafile->Height);
		param_space_bitmap->Canvas->Draw(0, 0, param_space_metafile);
		img->Picture->Assign(param_space_bitmap);
		delete param_space_metafile;
		delete param_space_bitmap;
	}

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::PrepareDataForParameterSpacePlot(vector<TParameterSpace> &best_value, vector<TParameterSpace> &inner_rim,
	vector<TParameterSpace> &outer_rim, vector<double> &param_min, vector<double> &param_max, int sdit, AnsiString &error)
{
	Application->ProcessMessages();

	TParameterMatrix pmat;
	vector<TParameterSpace>coll_all;
	vector<vector<TParameterSpace> >coll_prof_pruned;
	if (sdit == gutsSD)
	{
		pmat = PMatSD;
		coll_all = CalibrationParameterSpaceSD;
		coll_prof_pruned = CalibrationProfileLikelihoodsSD;
	}
	else
	{
		pmat = PMatIT;
		coll_all = CalibrationParameterSpaceIT;
		coll_prof_pruned = CalibrationProfileLikelihoodsIT;
	}

	if (coll_all.size() < 1)// || coll_prof_pruned.size() < 1)
	{
		error = "No existing parameter space for plotting parameter space plot!";
		return;
	}

	vector<int>ind_fit, ind_log;
	// indices to fitted and log-scale parameters (vectors)
	ind_fit.resize(0);
	ind_log.resize(0);
	int n_fit = 0;
	for (int p = 0; p < gutsPARAMETER_NO; p++)
	{
		if (pmat.Fit[p] == gutsFIT_PARAM)
		{
			ind_fit.push_back(p);
			n_fit++;
		}
		if (pmat.Scale[p] == gutsLOG_SCALE_PARAM)
			ind_log.push_back(p);
	}

	TParameterMatrix bnds_tmp = pmat;
	// copy parameter bounds to a temporary version
	for (int p = 0; p < ind_log.size(); p++)
	{
		bnds_tmp.Min[ind_log[p]] = log10(bnds_tmp.Min[ind_log[p]]);
		// and log10-transform these bounds where needed
		bnds_tmp.Max[ind_log[p]] = log10(bnds_tmp.Max[ind_log[p]]);
	}

	// We work here with the log-likelihood itself, so the chi2 criterion needs to be divided by 2.
	double chicrit_joint = 0.5 * gutsCRIT_TABLE[n_fit - 1];
	// criterion for joint 95% CI or parameters
	double chicrit_single = 0.5 * gutsCRIT_TABLE[0];
	// criterion for single-parameter CIs

	vector<TParameterSpace>coll_best;
	coll_best.resize(1);
	coll_best[0] = coll_all[0];
	// this is the best-fitting parameter set (<coll_all> is sorted)
	int ind_single = coll_all.size() - 1;
	// index to last element of sample that is still in inner rim
	int ind_fin95 = coll_all.size() - 1;
	// index to last element of sample that is still in joint CI
	for (int k = coll_all.size() - 1; k >= 0; k--)
		if (coll_all[k].Likelihood < coll_best[0].Likelihood + chicrit_single)
		{
			ind_single = k;
			break;
		}
	for (int k = coll_all.size() - 1; k >= 0; k--)
		if (coll_all[k].Likelihood < coll_best[0].Likelihood + chicrit_joint)
		{
			ind_fin95 = k;
			break;
		}

	vector<TParameterSpace>coll_ok;
	if (ind_single + 1 < coll_all.size())
		for (int i = ind_single + 1; i <= ind_fin95; i++)
			coll_ok.push_back(coll_all[i]);
	// use the outer rim for those points (the ghost symbols)

	vector<TParameterSpace>coll_inner;
	if (coll_all.size() > 1)
		for (int i = 1; i <= ind_single; i++)
			coll_inner.push_back(coll_all[i]); // inner rim for plotting

	vector<double>min_sample, max_sample;
	min_sample.resize(gutsPARAMETER_NO);
	max_sample.resize(gutsPARAMETER_NO);
	for (int p = 0; p < gutsPARAMETER_NO; p++)
	{
		min_sample[p] = HIGH_POS_DOUBLE;
		max_sample[p] = HIGH_NEG_DOUBLE;
	}
	for (int i = 0; i < coll_all.size(); i++)
	{
		for (int p = 0; p < gutsPARAMETER_NO; p++)
		{
			if (coll_all[i].Parameter[p] < min_sample[p])
				min_sample[p] = coll_all[i].Parameter[p];
			if (coll_all[i].Parameter[p] > max_sample[p])
				max_sample[p] = coll_all[i].Parameter[p];
		}
	}

	// We need to adjust these bounds for plotting profiles. Profile likelihoods
	// may extend beyond the range of the sample, and we need to see that.
	if (coll_prof_pruned.size() > 0)
	{
		for (int p = 0; p < ind_fit.size(); p++)
		{
			int parnum = ind_fit[p];
			for (int i = 0; i < coll_prof_pruned[parnum].size(); i++)
			{
				if (coll_prof_pruned[parnum][i].Parameter[parnum] < min_sample[parnum])
					min_sample[parnum] = coll_prof_pruned[parnum][i].Parameter[parnum];
				if (coll_prof_pruned[parnum][i].Parameter[parnum] > max_sample[parnum])
					max_sample[parnum] = coll_prof_pruned[parnum][i].Parameter[parnum];
			}
		}
	}

	// lower the minima and maxima by 5 %
	for (int p = 0; p < gutsPARAMETER_NO; p++)
	{
		min_sample[p] -= 0.05 * abs(min_sample[p]);
		max_sample[p] += 0.05 * abs(max_sample[p]);
	}

	best_value = coll_best;
	inner_rim = coll_inner;
	outer_rim = coll_ok;
	param_min = min_sample;
	param_max = max_sample;

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::PaintSingleLikelihoodProfilePlot(TMetafile *metafile, vector<TChartDataPoint> &best_value,
	vector<TChartDataPoint> &inner_rim, vector<TChartDataPoint> &outer_rim, vector<TChartDataPoint> &profile_likelihood,
	double min_x, double max_x, AnsiString x_label, AnsiString y_label)
{
	Application->ProcessMessages();

	// setting up the chart
	TChart *chart = new TChart(this);
	chart->AllowPanning = Vcltee::Teeprocs::pmNone;
	chart->BevelOuter = bvNone;
	chart->Color = clWhite;
	chart->View3D = false;
	chart->View3DWalls = false;
	chart->Legend->Visible = false;
	chart->LeftAxis->Grid->Visible = false;
	chart->LeftAxis->Axis->Width = 1;
	chart->LeftAxis->AxisValuesFormat = "#,##0.####";
	chart->LeftAxis->Minimum = 0;
	chart->LeftAxis->Texts->Font->Color = clBlack;
	chart->LeftAxis->Title->Font->Size = 12;
	chart->LeftAxis->Title->Text = y_label;

	chart->BottomAxis->Grid->Visible = false;
	chart->BottomAxis->Axis->Width = 1;
	chart->BottomAxis->AxisValuesFormat = "#,##0.####";
	chart->BottomAxis->Minimum = HIGH_NEG_DOUBLE;
	chart->BottomAxis->Maximum = max_x;
	chart->BottomAxis->Minimum = min_x;
	chart->BottomAxis->Texts->Font->Color = clBlack;
	chart->BottomAxis->Title->Font->Size = 12;
	chart->BottomAxis->Title->Text = x_label;

	// setting up the series
	chart->RemoveAllSeries();

	// inner and outer rims
	chart->AddSeries(new TPointSeries(chart));
	TPointSeries* ps1 = (TPointSeries*)chart->Series[0];
	ps1->Pointer->Style = psCircle;
	ps1->Pointer->Size = 2;
	ps1->Pointer->Color = clSkyBlue;
	ps1->Pointer->Pen->Color = clSkyBlue;

	chart->AddSeries(new TPointSeries(chart));
	TPointSeries* ps2 = (TPointSeries*)chart->Series[1];
	ps2->Pointer->Style = psCircle;
	ps2->Pointer->Size = 2;
	ps2->Pointer->Color = CL_CONF_BANDS_GREEN;
	ps2->Pointer->Pen->Color = CL_CONF_BANDS_GREEN;

	// horizontal lines
	chart->AddSeries(new TLineSeries(chart));
	TLineSeries* ls1 = (TLineSeries*)chart->Series[2];
	ls1->LinePen->Style = psSolid;
	ls1->SeriesColor = clBlack;

	chart->AddSeries(new TLineSeries(chart));
	TLineSeries* ls2 = (TLineSeries*)chart->Series[3];
	ls2->LinePen->Style = psDot;
	ls2->SeriesColor = clMedGray;

	chart->AddSeries(new TLineSeries(chart));
	TLineSeries* ls3 = (TLineSeries*)chart->Series[4];
	ls3->LinePen->Style = psDot;
	ls3->SeriesColor = clMedGray;

/*    // bounds (blue lines for future release)
	chart->AddSeries(new TLineSeries(chart));
	TLineSeries* ls5 = (TLineSeries*)chart->Series[5];
	ls5->LinePen->Style = psSolid;
	ls5->SeriesColor = clBlue;
	ls5->LinePen->Width = 2;

	chart->AddSeries(new TLineSeries(chart));
	TLineSeries* ls6 = (TLineSeries*)chart->Series[6];
	ls6->LinePen->Style = psSolid;
	ls6->SeriesColor = clBlue;
	ls6->LinePen->Width = 2;
*/
	// profile likelihood
	chart->AddSeries(new TLineSeries(chart));
	TLineSeries* ls4 = (TLineSeries*)chart->Series[5];
	ls4->LinePen->Style = psSolid;
	ls4->SeriesColor = clRed;
	ls4->LinePen->Width = 2;

	// best value
	chart->AddSeries(new TPointSeries(chart));
	TPointSeries* ps3 = (TPointSeries*)chart->Series[6];
	ps3->Pointer->Style = psCircle;
	ps3->Pointer->Size = 5;
	ps3->Pointer->Color = CL_CONF_BANDS_YELLOW;
	ps3->Pointer->Pen->Color = clBlack;


	// fill point data (inner and outer rim)
	vector<double> x_vals, y_vals;
	for (int i = 0; i < outer_rim.size(); i++)
	{
		chart->Series[0]->AddXY(outer_rim[i].X, outer_rim[i].Y - best_value[0].Y);
		x_vals.push_back(outer_rim[i].X);
		y_vals.push_back(outer_rim[i].Y - best_value[0].Y);
	}
	for (int i = 0; i < inner_rim.size(); i++)
	{
		chart->Series[1]->AddXY(inner_rim[i].X, inner_rim[i].Y - best_value[0].Y);
		x_vals.push_back(inner_rim[i].X);
		y_vals.push_back(inner_rim[i].Y - best_value[0].Y);
	}

	// fix chart dimensions to the current values
	chart->LeftAxis->AutomaticMaximum = false;
	chart->LeftAxis->AutomaticMinimum = false;
	chart->LeftAxis->Minimum = HIGH_NEG_DOUBLE;
	double new_y_max = *max_element(y_vals.begin(), y_vals.end());
	double new_y_min = *min_element(y_vals.begin(), y_vals.end());
	chart->LeftAxis->Maximum = new_y_max + (new_y_max - new_y_min)*0.05;
	chart->LeftAxis->Minimum = new_y_min - (new_y_max - new_y_min)*0.05;
	chart->BottomAxis->AutomaticMaximum = false;
	chart->BottomAxis->AutomaticMinimum = false;
	chart->BottomAxis->Minimum = HIGH_NEG_DOUBLE;
	double new_x_max = *max_element(x_vals.begin(), x_vals.end());
	double new_x_min = *min_element(x_vals.begin(), x_vals.end());
	chart->BottomAxis->Maximum = new_x_max + (new_x_max - new_x_min)*0.05;
	chart->BottomAxis->Minimum = new_x_min - (new_x_max - new_x_min)*0.05;

	// horizontal lines
	double chicrit_single = HIGH_NEG_DOUBLE;
	for (int i = 0; i < inner_rim.size(); i++)
		if (inner_rim[i].Y - best_value[0].Y > chicrit_single)
			chicrit_single = inner_rim[i].Y - best_value[0].Y;

	chart->Series[2]->AddXY(min_x, chicrit_single);
	chart->Series[2]->AddXY(max_x, chicrit_single);

	chart->Series[3]->AddXY(min_x, 0.5*gutsCRIT_PROP[0]);
	chart->Series[3]->AddXY(max_x, 0.5*gutsCRIT_PROP[0]);

	chart->Series[4]->AddXY(min_x, 0.5*gutsCRIT_PROP[1]);
	chart->Series[4]->AddXY(max_x, 0.5*gutsCRIT_PROP[1]);

	// blue bound lines
/*	chart->Series[5]->AddXY(min_x, chart->LeftAxis->Minimum);
	chart->Series[5]->AddXY(min_x, chart->LeftAxis->Maximum);

	chart->Series[6]->AddXY(max_x, chart->LeftAxis->Minimum);
	chart->Series[6]->AddXY(max_x, chart->LeftAxis->Maximum); */

	// red profile line
	for (int i = 0; i < profile_likelihood.size(); i++)
		chart->Series[5]->AddXY(profile_likelihood[i].X, profile_likelihood[i].Y - best_value[0].Y);

	// best value
	for (int i = 0; i < best_value.size(); i++)
		chart->Series[6]->AddXY(best_value[i].X, best_value[i].Y - best_value[0].Y);

	TRect rect;
	rect.Left = 0;
	rect.Top = 0;
	rect.Right = plotSINGLE_CHART_WIDTH;
	rect.Bottom = plotSINGLE_CHART_HEIGHT;
	metafile->Assign(chart->TeeCreateMetafile(true, rect));

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::PaintParameterSpacePlotLegend(TMetafile *metafile)
{
	Application->ProcessMessages();

	// setting up the chart
	TChart *chart = new TChart(this);
	chart->AllowPanning = Vcltee::Teeprocs::pmNone;
	chart->BevelOuter = bvNone;
	chart->Color = clWhite;
	chart->View3D = false;
	chart->View3DWalls = false;
	chart->Legend->Visible = true;
	chart->Legend->Shadow->Visible = false;

	chart->LeftAxis->Visible = false;
	chart->BottomAxis->Visible = false;

	// setting up the series
	chart->RemoveAllSeries();

	// joint 95% CI
	chart->AddSeries(new TPointSeries(chart));
	TPointSeries* ps1 = (TPointSeries*)chart->Series[chart->SeriesCount() - 1];
	ps1->Pointer->Style = psCircle;
	ps1->Pointer->Size = 3;
	ps1->Pointer->Color = clWhite;
	ps1->Pointer->Pen->Color = clSkyBlue;
	chart->Series[chart->SeriesCount() - 1]->Legend->Text = "joint 95% CI";

	// inner rim
	chart->AddSeries(new TPointSeries(chart));
	TPointSeries* ps2 = (TPointSeries*)chart->Series[chart->SeriesCount() - 1];
	ps2->Pointer->Style = psCircle;
	ps2->Pointer->Size = 4;
	ps2->Pointer->Color = CL_CONF_BANDS_GREEN;
	ps2->Pointer->Pen->Color = CL_CONF_BANDS_GREEN;
	chart->Series[chart->SeriesCount() - 1]->Legend->Text = "inner rim";

	// best fit set
	chart->AddSeries(new TPointSeries(chart));
	TPointSeries* ps3 = (TPointSeries*)chart->Series[chart->SeriesCount() - 1];
	ps3->Pointer->Style = psCircle;
	ps3->Pointer->Size = 5;
	ps3->Pointer->Color = CL_CONF_BANDS_YELLOW;
	ps3->Pointer->Pen->Color = clBlack;
	chart->Series[chart->SeriesCount() - 1]->Legend->Text = "best fit set";

	// joint 95 % CI
	chart->AddSeries(new TPointSeries(chart));
	TPointSeries* ps4 = (TPointSeries*)chart->Series[chart->SeriesCount() - 1];
	ps4->Pointer->Style = psCircle;
	ps4->Pointer->Size = 2;
	ps4->Pointer->Color = clSkyBlue;
	ps4->Pointer->Pen->Color = clSkyBlue;
	chart->Series[chart->SeriesCount() - 1]->Legend->Text = "joint 95% CI";

	// inner rim
	chart->AddSeries(new TPointSeries(chart));
	TPointSeries* ps5 = (TPointSeries*)chart->Series[chart->SeriesCount() - 1];
	ps5->Pointer->Style = psCircle;
	ps5->Pointer->Size = 2;
	ps5->Pointer->Color = CL_CONF_BANDS_GREEN;
	ps5->Pointer->Pen->Color = CL_CONF_BANDS_GREEN;
	chart->Series[chart->SeriesCount() - 1]->Legend->Text = "inner rim";

	// cut-off
	chart->AddSeries(new TLineSeries(chart));
	TLineSeries* ls1 = (TLineSeries*)chart->Series[chart->SeriesCount() - 1];
	ls1->LinePen->Style = psSolid;
	ls1->SeriesColor = clBlack;
	chart->Series[chart->SeriesCount() - 1]->Legend->Text = "cut-off";

	// profile refinement
	if (CalibrationProfileLikelihoodsSD.size() > 0)
	{
		chart->AddSeries(new TLineSeries(chart));
		TLineSeries* ls2 = (TLineSeries*)chart->Series[chart->SeriesCount() - 1];
		ls2->LinePen->Style = psSolid;
		ls2->SeriesColor = clRed;
		ls2->LinePen->Width = 2;
		chart->Series[chart->SeriesCount() - 1]->Legend->Text = "profile refinement";
	}

	TRect rect;
	rect.Left = 0;
	rect.Top = 0;
	rect.Right = plotSINGLE_CHART_WIDTH;
	rect.Bottom = plotSINGLE_CHART_HEIGHT;
	metafile->Assign(chart->TeeCreateMetafile(true, rect));

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::PaintSingleParameterSpacePlot(TMetafile *metafile, vector<TChartDataPoint> &best_value,
	vector<TChartDataPoint> &inner_rim, vector<TChartDataPoint> &outer_rim, double min_x, double max_x, double min_y,
	double max_y, AnsiString x_label, AnsiString y_label)
{
	Application->ProcessMessages();

	// Concept: Draw all wanted metafiles with defined width and height into a bigger metafile (i.e. on its canvas)

	// setting up the chart
	TChart *chart = new TChart(this);
	chart->AllowPanning = Vcltee::Teeprocs::pmNone;
	chart->BevelOuter = bvNone;
	chart->Color = clWhite;
	chart->View3D = false;
	chart->View3DWalls = false;
	chart->Legend->Visible = false;
	chart->LeftAxis->Grid->Visible = false;
	chart->LeftAxis->Minimum = HIGH_NEG_DOUBLE;
	chart->LeftAxis->Maximum = max_y;
	chart->LeftAxis->Minimum = min_y;
	chart->LeftAxis->Axis->Width = 1;
	chart->LeftAxis->AxisValuesFormat = "#,##0.####";
	chart->LeftAxis->Texts->Font->Color = clBlack;
	chart->LeftAxis->Title->Font->Size = 12;
	chart->LeftAxis->Title->Text = y_label;

	chart->BottomAxis->Grid->Visible = false;
	chart->BottomAxis->Minimum = HIGH_NEG_DOUBLE;
	chart->BottomAxis->Maximum = max_x;
	chart->BottomAxis->Minimum = min_x;
	chart->BottomAxis->Axis->Width = 1;
	chart->BottomAxis->AxisValuesFormat = "#,##0.####";
	chart->BottomAxis->Texts->Font->Color = clBlack;
	chart->BottomAxis->Title->Font->Size = 12;
	chart->BottomAxis->Title->Text = x_label;

	// setting up the series
	chart->RemoveAllSeries();
	chart->AddSeries(new TPointSeries(chart));
	TPointSeries* ps1 = (TPointSeries*)chart->Series[0];
	ps1->Pointer->Style = psCircle;
	ps1->Pointer->Size = 3;
	ps1->Pointer->Color = clWhite;
	ps1->Pointer->Pen->Color = clSkyBlue;

	chart->AddSeries(new TPointSeries(chart));
	TPointSeries* ps2 = (TPointSeries*)chart->Series[1];
	ps2->Pointer->Style = psCircle;
	ps2->Pointer->Size = 4;
	ps2->Pointer->Color = CL_CONF_BANDS_GREEN;
	ps2->Pointer->Pen->Color = CL_CONF_BANDS_GREEN;

	chart->AddSeries(new TPointSeries(chart));
	TPointSeries* ps3 = (TPointSeries*)chart->Series[2];
	ps3->Pointer->Style = psCircle;
	ps3->Pointer->Size = 5;
	ps3->Pointer->Color = CL_CONF_BANDS_YELLOW;
	ps3->Pointer->Pen->Color = clBlack;

	for (int i = 0; i < outer_rim.size(); i++)
	{
		chart->Series[0]->AddXY(outer_rim[i].X, outer_rim[i].Y);
	}
	for (int i = 0; i < inner_rim.size(); i++)
	{
		chart->Series[1]->AddXY(inner_rim[i].X, inner_rim[i].Y);
	}
	for (int i = 0; i < best_value.size(); i++)
	{
		chart->Series[2]->AddXY(best_value[i].X, best_value[i].Y);
	}

	TRect rect;
	rect.Left = 0;
	rect.Top = 0;
	rect.Right = plotSINGLE_CHART_WIDTH;
	rect.Bottom = plotSINGLE_CHART_HEIGHT;
	metafile->Assign(chart->TeeCreateMetafile(true, rect));

	return;
}
// ---------------------------------------------------------------------------

bool TMainForm::CheckAndGetValidationData(vector<TDataSet> &data)
{
	Application->ProcessMessages();

	if (!CheckValidationDataSet())
	{
		ValidationDataChecked = false;
		return false;
	}

	GetDataFromGrid(ValidationConcStringGrid, data[0].Concentration);
	// there is only one validation data set
	GetDataFromGrid(ValidationSurvStringGrid, data[0].Survival);
	data[0].ControlGroupIndex = ValidControlCombo->ItemIndex;
    data[0].Description = ValidationDataDescriptionRichEdit->Text;

	ValidationDataChecked = true;

	return true;
}
// ---------------------------------------------------------------------------

bool TMainForm::CheckValidationDataSet()
{
	// check the validation data set
	if (!CheckGridData(ValidationConcStringGrid, gutsCONC_DATA))
		return false;
	if (!CheckGridData(ValidationSurvStringGrid, gutsSURV_DATA))
		return false;
	if (!CheckConcSurvCompatibility(ValidationConcStringGrid, ValidationSurvStringGrid))
		return false;

	if (!CheckValidControlGroupAvailability())
		return false;

	ValidationDataChecked = true;

	return true;
}
// ---------------------------------------------------------------------------

bool TMainForm::CheckAndGetCalibrationData(vector<TDataSet> &data)
	// checks the calibration input and loads it to structure
{
	if (!CheckCalibrationDataSets())
	{
		CalibrationDataChecked = false;
		return false;
	}

	data.resize(NumberOfCalibrationDataSets);
	for (int i = 0; i < NumberOfCalibrationDataSets; i++)
	{
		GetDataFromGrid(CalibConcStringGrids[i], data[i].Concentration);
		GetDataFromGrid(CalibSurvStringGrids[i], data[i].Survival);
		data[i].ControlGroupIndex = CalibControlCombos[i]->ItemIndex;
		data[i].Description = CalibDataDescriptionRichEdits[i]->Text;
	}

	ExpertSettingsOn = ShowExpertSettingsCheckBox->Checked;

	CalibrationDataChecked = true;

	return true;
}
// ---------------------------------------------------------------------------

bool TMainForm::CheckCalibrationDataSets()
{
	// check all available calibration data sets
	for (int i = 0; i < NumberOfCalibrationDataSets; i++)
	{
		if (!CheckGridData(CalibConcStringGrids[i], gutsCONC_DATA))
			return false;
		if (!CheckGridData(CalibSurvStringGrids[i], gutsSURV_DATA))
			return false;
		if (!CheckConcSurvCompatibility(CalibConcStringGrids[i], CalibSurvStringGrids[i]))
			return false;
	}
	if (!CheckCalibControlGroupAvailability())
		return false;

	CalibrationDataChecked = true;

	return true;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::DisplayCalibrationDataButtonClick(TObject *Sender)
{
	// first CHECK and GET the calibration input data
	if (!CheckAndGetCalibrationData(TestCalibrationData))
	{
		return;
	}

	Screen->Cursor = crHourGlass;

	PaintInputData(InputInspectDataImage, CALIBRATION);
	CalibInspectDataScrollBox->Visible = true;
	InputDataPageControl2->ActivePage = InputInspectDataTab;

	Screen->Cursor = crDefault;

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::PaintInputData(TImage *img, int stage)
{
	vector<TDataSet>input_data;
	int number_of_sets;

	// CHECK and GET data
	if (stage == CALIBRATION)
	{
		input_data = TestCalibrationData;
		number_of_sets = TestCalibrationData.size();
		// read conc. unit
		ConcUnit = AnsiString(ConcUnitEdit->Text);
	}
	if (stage == VALIDATION)
	{
		input_data = TestValidationData;
		number_of_sets = 1;
	}
	if (stage == PREDICTION)
	{
		input_data = TestPredictionsData;
		number_of_sets = 1;
	}

	// first evaluate total number of groups for setting up the plotting area and max conc. for height of y-axis
	int max_group_number_per_set = 0;
	int total_group_number = 0;
	double max_time = 0;
	double max_conc = 0;
	double max_surv = 1.0;
	for (int i = 0; i < number_of_sets; i++)
	{
		if (input_data[i].Concentration.GroupNames.size() > max_group_number_per_set)
			max_group_number_per_set = input_data[i].Concentration.GroupNames.size();
		total_group_number += input_data[i].Concentration.GroupNames.size();
		// get maximum time value
		if (input_data[i].Concentration.Times[input_data[i].Concentration.Times.size() - 1] > max_time)
			max_time = input_data[i].Concentration.Times[input_data[i].Concentration.Times.size() - 1];
		if (input_data[i].Survival.Times[input_data[i].Survival.Times.size() - 1] > max_time)
			max_time = input_data[i].Survival.Times[input_data[i].Survival.Times.size() - 1];
		for (int j = 0; j < input_data[i].Concentration.GroupNames.size(); j++)
			for (int k = 0; k < input_data[i].Concentration.Times.size(); k++)
				if (input_data[i].Concentration.Values[j][k] > max_conc)
					max_conc = input_data[i].Concentration.Values[j][k];
	}

	int paint_exposure = 1;
	int paint_damage = 0;
	int paint_survival = 1;
	if (stage == PREDICTION)
		paint_survival = 0;
	int painting_rows_per_set = paint_exposure + paint_damage + paint_survival;

	// set plotting area
	int width = plotSINGLE_CHART_WIDTH;
	int total_width = plotSINGLE_CHART_WIDTH * max_group_number_per_set;
	int height = plotSINGLE_CHART_HEIGHT;
	int total_height = height * number_of_sets * painting_rows_per_set;

	// create metafiles and associated canvas (if neccessary) for entire exposure/damage/survival plots
	// entire metafile
	TMetafile *entire_metafile = new TMetafile();
	entire_metafile->SetSize(total_width, total_height);
	TMetafileCanvas *entire_metafile_canvas = new TMetafileCanvas(entire_metafile, 0);
	// exposure metafile
	TMetafile *exposure_metafile = new TMetafile();
	exposure_metafile->SetSize(total_width, height);
	// damage metafile
	TMetafile *damage_metafile = new TMetafile();
	damage_metafile->SetSize(total_width, height);
	// survival metafile
	TMetafile *survival_metafile = new TMetafile();
	survival_metafile->SetSize(total_width, height);

	// paint the plot rows
	int current_height = 0;
	for (int data_set_index = 0; data_set_index < number_of_sets; data_set_index++)
	{
		exposure_metafile->Clear();
		damage_metafile->Clear();
		survival_metafile->Clear();
		PaintDataSetExposureGraph(exposure_metafile, input_data[data_set_index].Concentration, data_set_index, max_time,
			max_conc);

		bool draw_measured_data = true;
		bool draw_predicted_data = false;

		PaintDataSetSurvivalGraph(survival_metafile, input_data[data_set_index].Survival, max_time, max_surv);

		// draw the plot rows to one metafile
		if (paint_exposure == 1)
		{
			entire_metafile_canvas->Draw(0, current_height, exposure_metafile);
			current_height += height;
		}
		if (paint_damage == 1)
		{
			entire_metafile_canvas->Draw(0, current_height, damage_metafile);
			current_height += height;
		}
		if (paint_survival == 1)
		{
			entire_metafile_canvas->Draw(0, current_height, survival_metafile);
			current_height += height;
		}
	}

	delete entire_metafile_canvas;
	delete exposure_metafile;
	delete damage_metafile;
	delete survival_metafile;

	// copy to clipboard
	img->Width = entire_metafile->Width;
	img->Height = entire_metafile->Height;
	img->Picture->Assign(entire_metafile);

	delete entire_metafile;

	return;
}
// ---------------------------------------------------------------------------

bool TMainForm::DataVecsToChartDataPoints(vector<TChartDataPoint> &plot_data, vector<double> &x, vector<double> &y,
	vector<double> &lower_y, vector<double> &upper_y)
{
	if (x.size() != y.size() || x.size() != lower_y.size() || x.size() != upper_y.size())
		return false;

	plot_data.resize(x.size());
	for (int i = 0; i < plot_data.size(); i++)
	{
		plot_data[i].X = x[i];
		plot_data[i].Y = y[i];
		plot_data[i].Lower_Y = lower_y[i];
		plot_data[i].Upper_Y = upper_y[i];
	}

	return true;
}
// ---------------------------------------------------------------------------

void TMainForm::PaintDataSetExposureGraph(TMetafile *exposure_metafile, TGridData &conc, int data_set_iterator, double &max_x,
	double &max_y)
{
	TMetafileCanvas *exposure_metafile_canvas = new TMetafileCanvas(exposure_metafile, 0);

	// paint all single exposure plots in line
	AnsiString title;
	bool first_plot;
	bool draw_x_labels = true;

	for (int j = 0; j < conc.GroupNames.size(); j++)
	{
		SetFirstPlotOption(first_plot, j);
		TMetafile *metafile = new TMetafile();
		title = "Data set " + AnsiString(data_set_iterator + 1) + "; " + conc.GroupNames[j];
		vector<TChartDataPoint>plot_data;
		DataVecsToChartDataPoints(plot_data, conc.Times, conc.Values[j], conc.Values[j], conc.Values[j]);
		PaintSingleExposureGraph(metafile, title.c_str(), plot_data, max_x, max_y, draw_x_labels, first_plot);
		exposure_metafile_canvas->Draw(j*plotSINGLE_CHART_WIDTH, 0, metafile);
		delete metafile;
	}

	// deleting the canvas is needed to fill metafile
	delete exposure_metafile_canvas;

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::PaintSingleExposureGraph(TMetafile *metafile, string title, vector<TChartDataPoint> &plot_data, double max_x,
	double max_y, bool draw_x_labels, bool &first_plot)
{
	// setting up the chart
	TChart *chart = new TChart(this);
	chart->AllowPanning = Vcltee::Teeprocs::pmNone;
	chart->BevelOuter = bvNone;
	chart->Color = clWhite;
	chart->View3D = false;
	chart->View3DWalls = false;
	chart->Legend->Visible = false;
	if (first_plot) // draw y-axis label only for first plot
			chart->LeftAxis->LabelsFont->Color = clBlack;
	else
		chart->LeftAxis->LabelsFont->Color = clWhite;
	chart->LeftAxis->Grid->Visible = false;
	chart->LeftAxis->AutomaticMinimum = false;
	chart->LeftAxis->Minimum = 0;
	chart->LeftAxis->AutomaticMaximum = false;
	chart->LeftAxis->Maximum = max_y + max_y * 0.05;
	chart->LeftAxis->Axis->Width = 1;
	chart->LeftAxis->AxisValuesFormat = "#,##0.####"; // "#,##0.0#";
	chart->LeftAxis->LabelsFont->Size = 10;
	chart->LeftAxis->Title->Text = ("Concentration [" + ConcUnitEdit->Text + "]").c_str();
	if (first_plot) // draw y-axis label only for first plot
	{
		chart->LeftAxis->Ticks->Color = clBlack;
		chart->LeftAxis->TicksInner->Color = clBlack;
		chart->LeftAxis->Texts->Font->Color = clBlack;
		chart->LeftAxis->Title->Font->Color = clBlack;
	}
	else
	{
		chart->LeftAxis->Ticks->Color = clWhite;
		chart->LeftAxis->TicksInner->Color = clWhite;
		chart->LeftAxis->Texts->Font->Color = clWhite;
		chart->LeftAxis->Title->Font->Color = clWhite;
	}
	chart->LeftAxis->Title->Font->Size = 12;
	chart->BottomAxis->Grid->Visible = false;
	chart->BottomAxis->Minimum = 0;
	chart->BottomAxis->AutomaticMaximum = false;
	chart->BottomAxis->Maximum = max_x + +max_x * 0.01;
	chart->BottomAxis->Axis->Width = 1;
	chart->BottomAxis->AxisValuesFormat = "#,##0.####";
	if (draw_x_labels)
	{
		chart->BottomAxis->Ticks->Color = clBlack;
		chart->BottomAxis->TicksInner->Color = clBlack;
		chart->BottomAxis->Texts->Font->Color = clBlack;
		chart->BottomAxis->Title->Font->Color = clBlack;
		chart->BottomAxis->Title->Text = "Time [days]";
		chart->BottomAxis->Title->Font->Size = 12;
	}
	else
	{
		chart->BottomAxis->Ticks->Color = clWhite;
		chart->BottomAxis->TicksInner->Color = clWhite;
		chart->BottomAxis->Texts->Font->Color = clWhite;
		chart->BottomAxis->Title->Font->Color = clWhite;
	}
	chart->BottomAxis->LabelsFont->Size = 10;
	chart->Title->Text->Clear();
	chart->Title->Text->Add(title.c_str());
	chart->Title->Font->Size = 10;
	chart->Title->Font->Color = clBlack;

	// setting up the series
	chart->RemoveAllSeries();
	// add area series
	chart->AddSeries(new TAreaSeries(chart));
	chart->Series[0]->Color = CL_EXPOSURE_BLUE;
	TAreaSeries *as = (TAreaSeries*)chart->Series[0];
	as->AreaLinesPen->Visible = false;

	// add data to chart series
	double last_valid_y = 0.0;
	for (int i = 0; i < plot_data.size(); i++)
		if (plot_data[i].Y != MISSING_DATA_NUM)
		{
			chart->Series[0]->AddXY(plot_data[i].X, plot_data[i].Y);
			last_valid_y = plot_data[i].Y;
		}
	chart->Series[0]->AddXY(max_x, last_valid_y);

	TRect rect;
	rect.Left = 0;
	rect.Top = 0;
	rect.Right = plotSINGLE_CHART_WIDTH;
	rect.Bottom = plotSINGLE_CHART_HEIGHT;
	metafile->Assign(chart->TeeCreateMetafile(true, rect));

	delete chart;

	return;
}
// ---------------------------------------------------------------------------

bool TMainForm::GetMaxXandYForPlottingAxis(vector<vector<TChartDataPoint> > &plot_data, double &max_x, double &max_y)
	// get the maximum value for x- and y-axis to harmonise plotting output
{
	max_x = 0.0; // all input values are positive (>= 0)
	max_y = 0.0; // all input values are positive (>= 0)

	// get maximum axis values
	for (int i = 0; i < plot_data.size(); i++)
		for (int j = 0; j < plot_data[i].size(); j++)
		{
			if (plot_data[i][j].X > max_x)
				max_x = plot_data[i][j].X;
			if (plot_data[i][j].Y > max_y || plot_data[i][j].Upper_Y > max_y)
				// upper_y must never be uninitialised, in case of "no CI" upper_y is set to y
					max_y = Max(plot_data[i][j].Y, plot_data[i][j].Upper_Y);
		}

	if (max_x == 0.0 || max_y == 0.0) // if max_x or max_y haven't changed
			return false;

	return true;
}
// ---------------------------------------------------------------------------

bool TMainForm::PaintDataSetGraphs(vector<vector<TChartDataPoint> > &measured_plot_data,
	vector<vector<TChartDataPoint> > &predicted_plot_data, int data_set_iterator, vector<AnsiString>group_names,
	TMetafile* metafile, TMetafileCanvas *mf_canvas, int stage, int data_type)
{
	// get max x and y values for axis painting
	double max_x, max_y;

	if (measured_plot_data.size() > 0 && !GetMaxXandYForPlottingAxis(measured_plot_data, max_x, max_y))
		return false;

	if (predicted_plot_data.size() > 0 && !GetMaxXandYForPlottingAxis(predicted_plot_data, max_x, max_y))
		return false;

	bool first_plot;
	AnsiString title;

	// adjust the painting setting according to stage
	bool draw_x_labels = true;
	bool draw_measured_data = true;
	bool draw_predicted_data = true;
	bool draw_wilson_score = true;
	bool draw_CI = true;
	if (stage == PREDICTION || stage == BATCH_PREDICTION)
	{
		draw_measured_data = false;
		draw_wilson_score = false;
		draw_CI = false;
	}

	if (data_type == gutsEXPOSURE)
	{
		for (int g = 0; g < measured_plot_data.size(); g++)
		{
			SetFirstPlotOption(first_plot, g);
			if (group_names.size() > g)
				title = "Data set " + AnsiString(data_set_iterator + 1) + "; " + group_names[g];
			else
				title = "Data set " + AnsiString(data_set_iterator + 1);
			PaintSingleExposureGraph(metafile, title.c_str(), measured_plot_data[g], max_x, max_y, draw_x_labels, first_plot);
			mf_canvas->Draw(g*plotSINGLE_CHART_WIDTH, 0, metafile);
		}
	}
	else if (data_type == gutsDAMAGE)
	{
		for (int g = 0; g < predicted_plot_data.size(); g++)
		{
			SetFirstPlotOption(first_plot, g);
			PaintSingleDamageGraph(metafile, predicted_plot_data[g], max_x, max_y, draw_x_labels, draw_CI, first_plot);
			mf_canvas->Draw(g*plotSINGLE_CHART_WIDTH, plotSINGLE_CHART_HEIGHT, metafile);
		}
	}
	else // data_type == gutsSURVIVAL
	{
		for (int g = 0; g < predicted_plot_data.size(); g++)
		{
			SetFirstPlotOption(first_plot, g);
			PaintSingleSurvivalGraph(metafile, measured_plot_data[g], predicted_plot_data[g], max_x, max_y, draw_x_labels,
				draw_measured_data, draw_predicted_data, draw_wilson_score, draw_CI, first_plot);
			mf_canvas->Draw(g*plotSINGLE_CHART_WIDTH, 2*plotSINGLE_CHART_HEIGHT, metafile);
		}
	}

	return true;
}
// ---------------------------------------------------------------------------

void TMainForm::SetFirstPlotOption(bool &first_plot, int g)
{
	if (g % MAX_NO_IMAGES_PER_ROW == 0)
		first_plot = true;
	else
		first_plot = false;

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::AdjustImageDimensions(TPreparedDataContainer &data_all, TImage *img, bool display_exposure, bool display_damage,
	bool display_measured_survival, bool display_predicted_survival)
{
	// determine the number of rows
	int row_num = 0;
	if (display_exposure)
		row_num++;
	if (display_damage)
		row_num++;
	if (display_measured_survival || display_predicted_survival)
		row_num++;
	row_num *= data_all.Survival.size();

	// determine the number of columns
	int col_num = 0;
	for (int d = 0; d < data_all.Concentration.size(); d++)
		if (data_all.Concentration[d].size() > col_num)
			col_num = data_all.Concentration[d].size();

	// calculate and set needed metafile dimensions
	img->Width = col_num * plotSINGLE_CHART_WIDTH;
	img->Height = row_num * plotSINGLE_CHART_HEIGHT;

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::AdjustMetafileDimensions(TPreparedDataContainer &data_all, int data_set_index, TMetafile *metafile,
	bool display_exposure, bool display_damage, bool display_measured_survival, bool display_predicted_survival)
{
	// determine the number of rows
	int row_num = 0;
	if (display_exposure)
		row_num++;
	if (display_damage)
		row_num++;
	if (display_measured_survival || display_predicted_survival)
		row_num++;

	// determine the number of columns
	int col_num = data_all.Concentration[data_set_index].size();

	// calculate and set needed metafile dimensions
	int width = col_num * plotSINGLE_CHART_WIDTH;
	int height = row_num * plotSINGLE_CHART_HEIGHT;
	metafile->SetSize(width, height);

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::PreparedDataToChartDataPoints(vector<vector<TChartDataPoint> > &plot_data,
	TPreparedDataContainer data_all, int data_set_index, int data_type)
{
	if (data_type == gutsEXPOSURE)
	{
		// backtransform prepared data to plot data
		double next_conc, test_conc;
		int count;

		// For the data set that was passed to the function (data_set_index):
		// Iterate over all concentration data structures;
		// i.e., iterate over all treatments of this “data set”

		for (int g=0; g<data_all.Concentration[data_set_index].size(); g++)
		{
			count = 0;
			// Within the concentration values data structure, 3 vectors exist:
			// Conc (the actual concentration data, “vector with concentrations for one treatment only”),
			// Times (the time at which the concentration was measured, “time vector with removed NaNs”) and
			// Slope  (“slope for concentration calculation in interval”).

			// Iterate over the measurements of a single treatment
			for (int t=0; t<data_all.Concentration[data_set_index][g].Slope.size()-1; t++)
			{
				// Extrapolate the concentration to the next time point, by:
				// Take the current concentration, add to it ...
				// (slope of next measurement) * (time difference between current and next measurement)
				next_conc = data_all.Concentration[data_set_index][g].Conc[t+count] +
							data_all.Concentration[data_set_index][g].Slope[t] *
							(data_all.Concentration[data_set_index][g].Times[t+count+1] -
							data_all.Concentration[data_set_index][g].Times[t+count]);
				// absolute of the concentration of the next time point
				test_conc = abs(data_all.Concentration[data_set_index][g].Conc[t+count+1]);

				// if the “measured” concentration of the next time point is larger than the extrapolated, then …
				if (abs(data_all.Concentration[data_set_index][g].Conc[t+count+1] - next_conc) > EPS)
				{
					// insert a new time point after the current time point
					data_all.Concentration[data_set_index][g].Times.insert(
							data_all.Concentration[data_set_index][g].Times.begin()+t+count,
							data_all.Concentration[data_set_index][g].Times[t+count+1]);
					// insert a new concentration value after the current concentration value
					data_all.Concentration[data_set_index][g].Conc.insert(
							data_all.Concentration[data_set_index][g].Conc.begin()+t+count,
							next_conc);
					count++;
				}
			}
		}
		plot_data.resize(data_all.Concentration[data_set_index].size());
	}
	else // data_type == gutsSURVIVAL
		plot_data.resize(data_all.Survival[data_set_index].size());

	// Finally, create the data structure for plotting
	// For each treatment ...
	for (int g = 0; g < plot_data.size(); g++)
	{
		// paste the Concentration time series from data_all: time points and concentration values (3 times)
		if (data_type == gutsEXPOSURE)
			DataVecsToChartDataPoints(plot_data[g],
					data_all.Concentration[data_set_index][g].Times,
					data_all.Concentration[data_set_index][g].Conc,
					data_all.Concentration[data_set_index][g].Conc,
					data_all.Concentration[data_set_index][g].Conc);
		else // data_type == gutsSURVIVAL
			// paste the Survival time series from data_all: time points and survivors (3 times)
			DataVecsToChartDataPoints(plot_data[g],
					data_all.Survival[data_set_index][g].Times,
					data_all.Survival[data_set_index][g].Survivors,
					data_all.Survival[data_set_index][g].Survivors,
					data_all.Survival[data_set_index][g].Survivors);
	}

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::SimulationOutputDataToChartDataPoints(vector<vector<TChartDataPoint> > &plot_data,
	vector<TSimulationOutput> &data_set_out, int data_type)
{
	plot_data.resize(data_set_out.size());

	if (data_type == gutsDAMAGE)
		for (int g = 0; g < plot_data.size(); g++)
			DataVecsToChartDataPoints(plot_data[g], data_set_out[g].Times, data_set_out[g].ScaledDamage,
			data_set_out[g].ScaledDamageLowerCI, data_set_out[g].ScaledDamageUpperCI);
	else // data_type == gutsSURVIVAL
		for (int g = 0; g < plot_data.size(); g++)
			DataVecsToChartDataPoints(plot_data[g], data_set_out[g].Times, data_set_out[g].SurvivalProb,
			data_set_out[g].SurvivalProbLowerCI, data_set_out[g].SurvivalProbUpperCI);

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::AppendGraphDataToText(AnsiString &graph_data_as_text, vector<vector<TChartDataPoint> > &plot_data,
	AnsiString header, vector<AnsiString> &group_names, bool print_upper_lower)
	// adds the plot data to a vector of AnsiStrings
{
	if (plot_data.size() < 1)
		return;

	AnsiString newline = "\r\n";

	graph_data_as_text += header + newline;

	if (print_upper_lower)
	{
		graph_data_as_text += "X";
		for (int g = 0; g < group_names.size(); g++)
			graph_data_as_text += "\t" + group_names[g] + " Y\t" + group_names[g] + " Lower Y\t" + group_names[g] + " Upper Y";
		graph_data_as_text += newline;
	}
	else
	{
		graph_data_as_text += "X";
		for (int g = 0; g < group_names.size(); g++)
			graph_data_as_text += "\t" + group_names[g] + " Y";
		graph_data_as_text += newline;
	}

	for (int t = 0; t < plot_data[0].size(); t++)
		if (print_upper_lower)
		{
			graph_data_as_text += AnsiString(plot_data[0][t].X);
			for (int g = 0; g < group_names.size(); g++)
				graph_data_as_text += "\t" + AnsiString(plot_data[g][t].Y) + "\t" + AnsiString(plot_data[g][t].Lower_Y) + "\t" +
					AnsiString(plot_data[g][t].Upper_Y);
			graph_data_as_text += newline;
		}
		else
		{
			graph_data_as_text += AnsiString(plot_data[0][t].X);
			for (int g = 0; g < group_names.size(); g++)
				graph_data_as_text += "\t" + AnsiString(plot_data[g][t].Y);
			graph_data_as_text += newline;
		}

	graph_data_as_text += newline;

	return;
}
// ---------------------------------------------------------------------------

bool TMainForm::DisplayGUTSGraphs(TPreparedDataContainer &data_all, vector<TSimulationOutput> &data_set_out, int data_set_index,
	TImage *img, bool display_exposure, bool display_damage, bool display_measured_survival, bool display_predicted_survival,
	int stage)
{
	if (data_set_index >= data_all.GroupNames.size() || data_set_index < 0)
	{
		MessageDlg("Error in DisplayGUTSGraphs(), data_set_index invalid!", mtError, TMsgDlgButtons() << mbOK, 0);
		// should never occur
		return false;
	}

	// Define metafile:
	TMetafile* metafile = new TMetafile();

	AdjustMetafileDimensions(data_all, data_set_index, metafile, display_exposure, display_damage, display_measured_survival,
		display_predicted_survival);
	TMetafileCanvas* mf_canvas = new TMetafileCanvas(metafile, 0);

	AnsiString graph_data_as_text;

	vector<vector<TChartDataPoint> >empty_plot_data;
	empty_plot_data.resize(0);

	// prepare exposure data
	vector<vector<TChartDataPoint> > exposure_plot_data;
	PreparedDataToChartDataPoints(exposure_plot_data, data_all, data_set_index, gutsEXPOSURE);

	// prepare damage data
	vector<vector<TChartDataPoint> >damage_plot_data;
	SimulationOutputDataToChartDataPoints(damage_plot_data, data_set_out, gutsDAMAGE);

	// prepare survival plot data
	vector<vector<TChartDataPoint> >measured_survival_plot_data;
	// measured survival
	PreparedDataToChartDataPoints(measured_survival_plot_data, data_all, data_set_index, gutsSURVIVAL);
	vector<vector<TChartDataPoint> >predicted_survival_plot_data;
	// predicted survival
	SimulationOutputDataToChartDataPoints(predicted_survival_plot_data, data_set_out, gutsSURVIVAL);

	int row_num = 0;
	bool print_upper_lower;
	// paint exposure
	if (display_exposure)
	{
		row_num++;
		PaintDataSetGraphs(exposure_plot_data, empty_plot_data, data_set_index, data_all.GroupNames[data_set_index], metafile,
			mf_canvas, stage, gutsEXPOSURE);
		print_upper_lower = false;
		AppendGraphDataToText(graph_data_as_text, exposure_plot_data, "Concentration data",
			data_all.GroupNames[data_set_index], print_upper_lower);
	}

	// paint damage
	if (display_damage)
	{
		row_num++;
		PaintDataSetGraphs(empty_plot_data, damage_plot_data, data_set_index, data_all.GroupNames[data_set_index], metafile,
			mf_canvas, stage, gutsDAMAGE);
		if (stage == CALIBRATION || stage == VALIDATION)
			print_upper_lower = true;
		else
			print_upper_lower = false;
		AppendGraphDataToText(graph_data_as_text, damage_plot_data, "Damage data", data_all.GroupNames[data_set_index],
			print_upper_lower);
	}

	// paint survival
	if (display_measured_survival || display_predicted_survival)
		// so far both are shown, this could be changed later if needed. Then these two settings would need to be passed on to PaintDataSetGraphs()
	{
		row_num++;
		PaintDataSetGraphs(measured_survival_plot_data, predicted_survival_plot_data, data_set_index,
			data_all.GroupNames[data_set_index], metafile, mf_canvas, stage, gutsSURVIVAL);
        if (stage == CALIBRATION || stage == VALIDATION)
			print_upper_lower = true;
		else
			print_upper_lower = false;
		if (display_measured_survival)
			AppendGraphDataToText(graph_data_as_text, measured_survival_plot_data, "Measured survival data",
				data_all.GroupNames[data_set_index], print_upper_lower);
		if (display_predicted_survival)
			AppendGraphDataToText(graph_data_as_text, predicted_survival_plot_data, "Predicted survival data",
				data_all.GroupNames[data_set_index], print_upper_lower);
	}

	delete mf_canvas;

	// set image size
	img->Width = data_all.Concentration[data_set_index].size() * plotSINGLE_CHART_WIDTH;
	img->Height = row_num * plotSINGLE_CHART_HEIGHT;

	// assign picture
	img->Picture->Assign(metafile);

	// store data in image data
	img->Hint = graph_data_as_text;

	img->OnMouseDown = ImageMouseDown;

	delete metafile;

	return true;
}
// ---------------------------------------------------------------------------

void TMainForm::PaintDataSetDamageGraph(TMetafile *damage_metafile, TGridData &damage, TGridData &lower_damage,
	TGridData &upper_damage, double &max_x, double &max_y)
{
	TMetafileCanvas *damage_metafile_canvas = new TMetafileCanvas(damage_metafile, 0);

	// paint all single exposure plots in line
	bool draw_x_labels = false;
	bool draw_CI = true;
	bool first_plot;

	for (int j = 0; j < damage.GroupNames.size(); j++)
	{
		TMetafile *metafile;
		metafile = new TMetafile();
		vector<TChartDataPoint>plot_data;
		DataVecsToChartDataPoints(plot_data, damage.Times, damage.Values[j], damage.Values[j], damage.Values[j]);
        SetFirstPlotOption(first_plot, j);
		PaintSingleDamageGraph(metafile, plot_data, max_x, max_y, draw_x_labels, draw_CI, first_plot);
		// PaintSingleDamageGraph(metafile, damage.Times, damage.Values[j], lower_damage.Values[j], upper_damage.Values[j], max_x, max_y, false, true, true, first_plot);
		damage_metafile_canvas->Draw(j*plotSINGLE_CHART_WIDTH, 0, metafile);
		delete metafile;
	}

	// deleting the canvas is needed to fill metafile
	delete damage_metafile_canvas;

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::PaintSingleDamageGraph(TMetafile *metafile, vector<TChartDataPoint> &plot_data, double max_x, double max_y,
	bool draw_x_labels, bool draw_CI, bool &first_plot)
{
	// setting up the chart
	TChart *chart = new TChart(this);
	chart->AllowPanning = Vcltee::Teeprocs::pmNone;
	chart->BevelOuter = bvNone;
	chart->Color = clWhite;
	chart->View3D = false;
	chart->View3DWalls = false;
	chart->Legend->Visible = false;
	if (first_plot) // draw y-axis label only for first plot
			chart->LeftAxis->LabelsFont->Color = clBlack;
	else
		chart->LeftAxis->LabelsFont->Color = clWhite;
	chart->LeftAxis->Grid->Visible = false;
	chart->LeftAxis->AutomaticMinimum = false;
	chart->LeftAxis->Minimum = 0;
	chart->LeftAxis->AutomaticMaximum = false;
	chart->LeftAxis->Maximum = max_y + max_y * 0.05;
	chart->LeftAxis->Axis->Width = 1;
	chart->LeftAxis->AxisValuesFormat = "#,##0.####";
	chart->LeftAxis->LabelsFont->Size = 10;
	chart->LeftAxis->Title->Text = "Scaled damage [" + ConcUnitEdit->Text + "]";
	if (first_plot) // draw y-axis label only for first plot
	{
		chart->LeftAxis->Ticks->Color = clBlack;
		chart->LeftAxis->TicksInner->Color = clBlack;
		chart->LeftAxis->Texts->Font->Color = clBlack;
		chart->LeftAxis->Title->Font->Color = clBlack;
	}
	else
	{
		chart->LeftAxis->Ticks->Color = clWhite;
		chart->LeftAxis->TicksInner->Color = clWhite;
		chart->LeftAxis->Texts->Font->Color = clWhite;
		chart->LeftAxis->Title->Font->Color = clWhite;
	}
	chart->LeftAxis->Title->Font->Size = 12;
	chart->BottomAxis->Grid->Visible = false;
	chart->BottomAxis->Minimum = 0;
	chart->BottomAxis->AutomaticMaximum = false;
	chart->BottomAxis->Maximum = max_x + +max_x * 0.01;
	chart->BottomAxis->Axis->Width = 1;
	chart->BottomAxis->AxisValuesFormat = "#,##0.####";
	if (draw_x_labels)
	{
		chart->BottomAxis->Ticks->Color = clBlack;
		chart->BottomAxis->TicksInner->Color = clBlack;
		chart->BottomAxis->Texts->Font->Color = clBlack;
		chart->BottomAxis->Title->Font->Color = clBlack;
	}
	else
	{
		chart->BottomAxis->Ticks->Color = clWhite;
		chart->BottomAxis->TicksInner->Color = clWhite;
		chart->BottomAxis->Texts->Font->Color = clWhite;
		chart->BottomAxis->Title->Font->Color = clWhite;
	}
	chart->BottomAxis->LabelsFont->Size = 10;
	chart->BottomAxis->Title->Text = "Time [days]";
	chart->BottomAxis->Title->Font->Size = 12;
	chart->Title->Text->Clear();

	// setting up the series
	chart->RemoveAllSeries();
	// add area series
	chart->AddSeries(new TAreaSeries(chart));
	chart->Series[0]->Color = CL_CONF_BANDS_GREEN;
	TAreaSeries *as1 = (TAreaSeries*)chart->Series[0];
	as1->AreaLinesPen->Visible = false;
	as1->LinePen->Style = psSolid;
	as1->LinePen->Color = clMedGray;
	// add area series
	chart->AddSeries(new TAreaSeries(chart));
	chart->Series[1]->Color = clWhite;
	TAreaSeries *as2 = (TAreaSeries*)chart->Series[1];
	as2->AreaLinesPen->Visible = false;
	as2->LinePen->Style = psSolid;
	as2->LinePen->Color = clMedGray;
	// add line series
	chart->AddSeries(new TLineSeries(chart));
	chart->Series[2]->Color = clBlack;
	chart->Series[2]->Pen->Style = psSolid;

	// add data to chart series
	for (int i = 0; i < plot_data.size(); i++)
	{
		if (plot_data[i].Upper_Y != MISSING_DATA_NUM)
			chart->Series[0]->AddXY(plot_data[i].X, plot_data[i].Upper_Y);
		if (plot_data[i].Lower_Y != MISSING_DATA_NUM)
			chart->Series[1]->AddXY(plot_data[i].X, plot_data[i].Lower_Y);
		if (plot_data[i].Y != MISSING_DATA_NUM)
			chart->Series[2]->AddXY(plot_data[i].X, plot_data[i].Y);
	}

	TRect rect;
	rect.Left = 0;
	rect.Top = 0;
	rect.Right = plotSINGLE_CHART_WIDTH;
	rect.Bottom = plotSINGLE_CHART_HEIGHT;
	metafile->Assign(chart->TeeCreateMetafile(true, rect));

	delete chart;

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::PaintDataSetSurvivalGraph(TMetafile *survival_metafile, TGridData &surv, double &max_x, double &max_y)
{
	TMetafileCanvas *survival_metafile_canvas = new TMetafileCanvas(survival_metafile, 0);

	// paint all single exposure plots in line
	bool draw_x_labels = true;
	bool draw_measured_data = true;
	bool draw_predicted_data = false;
	bool draw_wilson_score = true;
	bool draw_CI = true;

	bool first_plot;

	for (int j = 0; j < surv.GroupNames.size(); j++)
	{
		TMetafile *metafile;
		metafile = new TMetafile();
		vector<TChartDataPoint>measured_plot_data;
		DataVecsToChartDataPoints(measured_plot_data, surv.Times, surv.Values[j], surv.Values[j], surv.Values[j]);
		vector<TChartDataPoint>empty_predicted_plot_data;
        SetFirstPlotOption(first_plot, j);
		PaintSingleSurvivalGraph(metafile, measured_plot_data, empty_predicted_plot_data, max_x, max_y, draw_x_labels,
			draw_measured_data, draw_predicted_data, draw_wilson_score, draw_CI, first_plot);
		survival_metafile_canvas->Draw(j*plotSINGLE_CHART_WIDTH, 0, metafile);
		delete metafile;
	}

	// deleting the canvas is needed to fill metafile
	delete survival_metafile_canvas;

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::PaintSingleSurvivalGraph(TMetafile *metafile, vector<TChartDataPoint> &measured_plot_data,
	vector<TChartDataPoint> &predicted_plot_data, double max_x, double max_y, bool draw_x_labels, bool draw_measured_data,
	bool draw_predicted_data, bool draw_wilson_score, bool draw_CI, bool &first_plot)
{
	// setting up the chart
	TChart *chart = new TChart(this);
	chart->AllowPanning = Vcltee::Teeprocs::pmNone;
	chart->BevelOuter = bvNone;
	chart->Color = clWhite;
	chart->View3D = false;
	chart->View3DWalls = false;
	chart->Legend->Visible = false;
	if (first_plot) // draw y-axis label only for first plot
		chart->LeftAxis->LabelsFont->Color = clBlack;
	else
		chart->LeftAxis->LabelsFont->Color = clWhite;
	chart->LeftAxis->Grid->Visible = false;
	chart->LeftAxis->AutomaticMinimum = false;
	chart->LeftAxis->Minimum = 0;
	chart->LeftAxis->AutomaticMaximum = false;
	chart->LeftAxis->Maximum = max_y + max_y * 0.05;
	chart->LeftAxis->Axis->Width = 1;
	chart->LeftAxis->AxisValuesFormat = "#,##0.####";
	chart->LeftAxis->LabelsFont->Size = 10;
	chart->LeftAxis->Title->Text = "Survival [0..1]";
	if (first_plot) // draw y-axis label only for first plot
	{
		chart->LeftAxis->Ticks->Color = clBlack;
		chart->LeftAxis->TicksInner->Color = clBlack;
		chart->LeftAxis->Texts->Font->Color = clBlack;
		chart->LeftAxis->Title->Font->Color = clBlack;
	}
	else
	{
		chart->LeftAxis->Ticks->Color = clWhite;
		chart->LeftAxis->TicksInner->Color = clWhite;
		chart->LeftAxis->Texts->Font->Color = clWhite;
		chart->LeftAxis->Title->Font->Color = clWhite;
	}
	chart->LeftAxis->Title->Font->Size = 12;
	chart->BottomAxis->Grid->Visible = false;
	chart->BottomAxis->Minimum = 0;
	chart->BottomAxis->AutomaticMaximum = false;
	chart->BottomAxis->Maximum = max_x + +max_x * 0.01;
	chart->BottomAxis->Axis->Width = 1;
	chart->BottomAxis->AxisValuesFormat = "#,##0.####";
	if (draw_x_labels)
	{
		chart->BottomAxis->Ticks->Color = clBlack;
		chart->BottomAxis->TicksInner->Color = clBlack;
		chart->BottomAxis->Texts->Font->Color = clBlack;
		chart->BottomAxis->Title->Font->Color = clBlack;
	}
	else
	{
		chart->BottomAxis->Ticks->Color = clWhite;
		chart->BottomAxis->TicksInner->Color = clWhite;
		chart->BottomAxis->Texts->Font->Color = clWhite;
		chart->BottomAxis->Title->Font->Color = clWhite;
	}
	chart->BottomAxis->LabelsFont->Size = 10;
	chart->BottomAxis->Title->Text = "Time [days]";
	chart->BottomAxis->Title->Font->Size = 12;
	chart->Title->Text->Clear();

	// setting up the series
	chart->RemoveAllSeries();

	// PLOT PREDICTED SURVIVAL AND CI BANDS
	// add area series
	chart->AddSeries(new TAreaSeries(chart));
	chart->Series[0]->Color = CL_CONF_BANDS_GREEN;
	TAreaSeries *as1 = (TAreaSeries*)chart->Series[0];
	as1->AreaLinesPen->Visible = false;
	as1->LinePen->Style = psSolid;
	as1->LinePen->Color = clMedGray;
	// add area series
	chart->AddSeries(new TAreaSeries(chart));
	chart->Series[1]->Color = clWhite;
	TAreaSeries *as2 = (TAreaSeries*)chart->Series[1];
	as2->AreaLinesPen->Visible = false;
	as2->LinePen->Style = psSolid;
	as2->LinePen->Color = clMedGray;
	// add line series
	chart->AddSeries(new TLineSeries(chart));
	chart->Series[2]->Color = clBlack;
	chart->Series[2]->Pen->Style = psSolid;
	if (draw_predicted_data)
		for (int i = 0; i < predicted_plot_data.size(); i++)
		{
			if (predicted_plot_data[i].Upper_Y != MISSING_DATA_NUM)
				chart->Series[0]->AddXY(predicted_plot_data[i].X, predicted_plot_data[i].Upper_Y);
			if (predicted_plot_data[i].Lower_Y != MISSING_DATA_NUM)
				chart->Series[1]->AddXY(predicted_plot_data[i].X, predicted_plot_data[i].Lower_Y);
			if (predicted_plot_data[i].Y != MISSING_DATA_NUM)
				chart->Series[2]->AddXY(predicted_plot_data[i].X, predicted_plot_data[i].Y);
		}

	// PLOT MEASURED DATA
	// dotted line
	chart->AddSeries(new TLineSeries(chart));
	chart->Series[3]->Color = clBlack;
	chart->Series[3]->Pen->Style = psDot;
	// points
	chart->AddSeries(new TPointSeries(chart));
	TPointSeries* ps = (TPointSeries*)chart->Series[4];
	ps->Pointer->Style = psCircle;
	ps->Pointer->Size = 3;
	ps->Pointer->Color = clBlack;

	// add data to chart series
	vector<double>ws;
	if (draw_measured_data)
		for (int i = 0; i < measured_plot_data.size(); i++)
			if (measured_plot_data[i].Y != MISSING_DATA_NUM)
			{
				if (!draw_predicted_data)
					chart->Series[3]->AddXY(measured_plot_data[i].X, measured_plot_data[i].Y / measured_plot_data[0].Y);
				chart->Series[4]->AddXY(measured_plot_data[i].X, measured_plot_data[i].Y / measured_plot_data[0].Y);
				if (draw_wilson_score)
					if (i > 0)
					{
						ws = CalcWilsonScore(measured_plot_data[0].Y, measured_plot_data[i].Y);
						chart->AddSeries(new TLineSeries(chart));
						chart->Series[chart->SeriesCount() - 1]->Color = clBlack;
						chart->Series[chart->SeriesCount() - 1]->AddXY(measured_plot_data[i].X, ws[0]);
						chart->Series[chart->SeriesCount() - 1]->AddXY(measured_plot_data[i].X, ws[1]);
					}
			}

	// assign chart image to metafile
	TRect rect;
	rect.Left = 0;
	rect.Top = 0;
	rect.Right = plotSINGLE_CHART_WIDTH;
	rect.Bottom = plotSINGLE_CHART_HEIGHT;
	metafile->Assign(chart->TeeCreateMetafile(true, rect));

	delete chart;

	return;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::ChartxMouseDown(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y)
{
	if (Button != mbRight)
		return;

	TPoint mouse_position;
	mouse_position = Mouse->CursorPos;
	CopyDataAndImagePopupMenu->Popup(mouse_position.x, mouse_position.y);

	return;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::ImageMouseDown(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y)
{
	if (Button != mbRight)
		return;

	Image = (TImage*)Sender;
	TPoint mouse_position;
	mouse_position = Mouse->CursorPos;
	CopyDataAndImagePopupMenu->Popup(mouse_position.x, mouse_position.y);

	return;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::CalibrationSDOutputParamImageMouseDown(TObject *Sender, TMouseButton Button, TShiftState Shift,
	int X, int Y)
{
	if (Button != mbRight)
		return;

	Image = (TImage*)Sender;

	TPoint mouse_position;
	mouse_position = Mouse->CursorPos;
	CopyDataAndImagePopupMenu->Popup(mouse_position.x, mouse_position.y);

	return;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::FormDestroy(TObject *Sender)
{
	FormatSettings.DecimalSeparator = OldDecimalSeparator;
	delete BatchPredictionSurvivalImageSD;
	delete BatchPredictionSurvivalImageIT;
	delete BatchPredictionMultiFacImageSD;
	delete BatchPredictionMultiFacImageIT;

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::CopyFromGridToClipboard(TStringGrid *grid)
{
	AnsiString as = "";
	for (int r = grid->Selection.Top; r <= grid->Selection.Bottom; r++)
	{
		for (int c = grid->Selection.Left; c <= grid->Selection.Right; c++)
		{
			as += grid->Cells[c][r]; // append cell content to string
			if (c != grid->Selection.Right)
				as += '\t'; // add tab as element delimiter
		}
		if (r < grid->Selection.Bottom)
			as += "\r\n"; // add linebreak after each row (except last row)
	}
	Clipboard()->SetTextBuf(WideString(as).c_bstr());
	// copy string to clipboard

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::CutFromGridToClipboard(TStringGrid *grid)
{
	AnsiString as = "";

	// go from top selected row to bottom
	for (int r = grid->Selection.Top; r <= grid->Selection.Bottom; r++)
	{
		// go from left selected column to right
		for (int c = grid->Selection.Left; c <= grid->Selection.Right; c++)
		{
			as += grid->Cells[c][r]; // append cell content to string
			grid->Cells[c][r] = ""; // erase cell content ("cut")
			if (c != grid->Selection.Right)
				as += '\t'; // add tab as element delimiter
		}
		if (r < grid->Selection.Bottom)
			as += "\r\n"; // add linebreak after each row (except last row)
	}
	Clipboard()->SetTextBuf(WideString(as).c_bstr());
	// copy string to clipboard

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::PasteFromClipboardToGrid(TStringGrid *grid)
{
	string s = AnsiString(Clipboard()->AsText).c_str();
	string line_break = "\r\n";
	string tab_break = "\t";
	int string_pos;
	vector<string>sv;

	// first split the string into rows/lines with the line_break delimiter \r\n
	while ((string_pos = s.find(line_break)) != string::npos)
		// find first occurence of delimiter in string
	{
		sv.push_back(s.substr(0, string_pos));
		// push back substring until delimiter is reached
		s.erase(0, string_pos + line_break.length());
		// erase the pushed back substring from the string
	}
	sv.push_back(s); // push back last segment of the string

	// adjust number of rows if neccessary
	if (grid->RowCount < grid->Selection.Top + sv.size())
		grid->RowCount = grid->Selection.Top + sv.size();

	// second split the row strings into single elements with the tab_break delimiter \t
	for (int r = 0; r < sv.size(); r++)
	{
		int c = 0;
		while ((string_pos = sv[r].find(tab_break)) != string::npos)
		{
			if (grid->ColCount < grid->Selection.Left + c)
				grid->ColCount = grid->Selection.Left + c;
			// adjust number of columns if neccessary
			grid->Cells[c + grid->Selection.Left][r + grid->Selection.Top] = sv[r].substr(0, string_pos).c_str();
			sv[r].erase(0, string_pos + tab_break.length());
			c++;
		}
		if (grid->ColCount < grid->Selection.Left + c + 1)
			grid->ColCount = grid->Selection.Left + c + 1;
		// adjust number of columns if neccessary
		grid->Cells[c + grid->Selection.Left][r + grid->Selection.Top] = sv[r].c_str();
	}

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::DeleteFromGrid(TStringGrid *grid)
{
	// delete every cell in selected area
	for (int r = grid->Selection.Top; r <= grid->Selection.Bottom; r++) // go from top selected row to bottom
		for (int c = grid->Selection.Left; c <= grid->Selection.Right; c++) // go from left selected column to right
				grid->Cells[c][r] = ""; // erase cell content ("cut")

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::SelectAllCellsInGrid(TStringGrid *grid)
{
	// select every cell in grid (except fixed rows and columns)
	if (grid->EditorMode == false)
	{
		TGridRect rect;
		rect.Left = grid->FixedCols;
		rect.Top = grid->FixedRows;
		rect.Right = grid->ColCount - 1;
		rect.Bottom = grid->RowCount - 1;
		grid->Selection = rect;
	}

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::JumpToNextOrEndOfBlockInGrid(TStringGrid *grid, WORD &key)
{

	TGridRect rect;
	int col = ColInGrid;
	// used global variables instead of grid->Col, because Ctrl+arrow already triggers a change of grid->Call before function call
	int row = RowInGrid;
	if (key == 37 && col > grid->FixedCols) // left arrow
	{
		for (int c = col - 1; c >= grid->FixedCols; c--)
			// either first column reached or (curr. filled and (prev. empty or next empty))
			if (c == grid->FixedCols || (grid->Cells[c][row] != "" && (grid->Cells[c + 1][row] == "" ||
				grid->Cells[c - 1][row] == "")))
			{
				grid->Col = c;
				break;
			}
	}
	else if (key == 39 && col < grid->ColCount - 1) // right arrow
	{
		for (int c = col + 1; c < grid->ColCount; c++)
			// either last column reached or (curr. filled and (prev. empty or next empty))
			if (c == grid->ColCount - 1 || (grid->Cells[c][row] != "" && (grid->Cells[c + 1][row] == "" ||
				grid->Cells[c - 1][row] == "")))
			{
				grid->Col = c;
				break;
			}
	}
	else if (key == 38 && row > grid->FixedRows) // up arrow
	{
		for (int r = row - 1; r >= grid->FixedRows; r--)
			// either first row reached or (curr. filled and (prev. empty or next empty))
			if (r == grid->FixedRows || (grid->Cells[col][r] != "" && (grid->Cells[col][r + 1] == "" ||
				grid->Cells[col][r - 1] == "")))
			{
				grid->Row = r;
				break;
			}
	}
	else if (key == 40 && row < grid->RowCount - 1) // down arrow
	{
		for (int r = row + 1; r < grid->RowCount; r++)
			// either last row reached or (curr. filled and (prev. empty or next empty))
			if (r == grid->RowCount - 1 || (grid->Cells[col][r] != "" && (grid->Cells[col][r + 1] == "" ||
				grid->Cells[col][r - 1] == "")))
			{
				grid->Row = r;
				break;
			}
	}
	else
		return;

	grid->SetFocus();

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::EvaluatePressedKeysInGrid(TStringGrid *grid, WORD &key, TShiftState &shift)
{
	// evaluate the key combination when a key was pressed
	System::WideChar system_key = (System::WideChar)key;

	if (shift.Contains(ssCtrl)) // test if Ctrl is pressed
	{
		if (key == 37 || key == 38 || key == 39 || key == 40)
			// arrow keys (left, up, right, down)
		{
			if (shift.Contains(ssShift)) // Select all cells in wished direction
			{
				// do nothing
			}
			else
				JumpToNextOrEndOfBlockInGrid(grid, key);
			// Jump to border cell of wished direction
		}
		if (key == 65) // 65=A --> Select all	= Ctrl+A
				SelectAllCellsInGrid(grid);
		if (key == 67) // 67=C --> Copy 		= Ctrl+C
				CopyFromGridToClipboard(grid);
		if (key == 88) // 88=X --> Cut  		= Ctrl+X
				CutFromGridToClipboard(grid);
		if (key == 86) // 86=V --> Paste       = Ctrl+V
				PasteFromClipboardToGrid(grid);
	}
	else if (key == 46 && !CheckIfTopLeftCellEdited(grid, system_key)) // 8=backspace or 46=delete pressed
			DeleteFromGrid(grid);
	else if (key == 113) // 113=F2 --> activate editor mode
			grid->EditorMode = true;
	else if (key == 13)
	{
		grid->Row++;
		grid->EditorMode = false;
	}
	else
		return;

	return;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::SG1KeyDown(TObject *Sender, WORD &Key, TShiftState Shift)
{
	Reset(false);

	TStringGrid *grid = (TStringGrid*)Sender;

	// remember current position and selection in grid when Ctrl or another key is pressed down
	if (Shift.Contains(ssCtrl))
	{
		ColInGrid = grid->Col;
		RowInGrid = grid->Row;
		SelectionInGrid = grid->Selection;
	}

	TShiftState shift = Shift;
	WORD key = Key;

	EvaluatePressedKeysInGrid(grid, key, shift);

	return;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::ValidationGridKeyDown(TObject *Sender, WORD &Key, TShiftState Shift)
{
	ClearValidationResults();

	TStringGrid *grid = (TStringGrid*)Sender;

	// remember current position and selection in grid when Ctrl or another key is pressed down
	if (Shift.Contains(ssCtrl))
	{
		ColInGrid = grid->Col;
		RowInGrid = grid->Row;
		SelectionInGrid = grid->Selection;
	}

	TShiftState shift = Shift;
	WORD key = Key;

	EvaluatePressedKeysInGrid(grid, key, shift);

	return;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::PredictionsGridKeyDown(TObject *Sender, WORD &Key, TShiftState Shift)
{
	ClearPredictionsResults();

	PredictionsDataFileName = "Data edited manually.";

	TStringGrid *grid = (TStringGrid*)Sender;

	// remember current position and selection in grid when Ctrl or another key is pressed down
	if (Shift.Contains(ssCtrl))
	{
		ColInGrid = grid->Col;
		RowInGrid = grid->Row;
		SelectionInGrid = grid->Selection;
	}

	TShiftState shift = Shift;
	WORD key = Key;

	EvaluatePressedKeysInGrid(grid, key, shift);

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::GetDataFromGrid(TStringGrid *grid, TGridData &grid_data)
{


	string s;
	vector<string>sv;
	int number_of_rows, number_of_columns;

	// ********** get dimension of data in grid **********
	// only filled cells of first row and column are considered to determine
	// the dimension, i.e. data without a header in row or column is ignored
	number_of_rows = 0;
	number_of_columns = 0;
	for (int r = grid->RowCount - 1; r != 0; r--)
		if (grid->Cells[0][r] != "")
		{
			number_of_rows = r + 1;
			break;
		}
	for (int c = grid->ColCount - 1; c != 0; c--)
		if (grid->Cells[c][0] != "")
		{
			number_of_columns = c + 1;
			break;
		}
	// resize vectors
	grid_data.Times.resize(number_of_rows - 1);
	grid_data.GroupNames.resize(number_of_columns - 1);
	grid_data.Values.resize(number_of_columns - 1);
	for (int i = 0; i < grid_data.Values.size(); i++)
		grid_data.Values[i].resize(number_of_rows - 1);

	// ********** write grid data to structure **********
	// get time unit
	s = AnsiString(grid->Cells[0][0]).c_str();
	sv = SplitString(s, "[");
	sv = SplitString(sv[sv.size() - 1], "]");
	grid_data.TimeUnit = sv[0];
	// get measurement times
	for (int r = 1; r < grid_data.Times.size() + 1; r++)
		grid_data.Times[r - 1] = _wtof(grid->Cells[0][r].c_str());
	// get group names
	for (int c = 1; c < grid_data.GroupNames.size() + 1; c++)
		grid_data.GroupNames[c - 1] = grid->Cells[c][0];
	// get measured values
	for (int r = 0; r < grid_data.Times.size(); r++)
		for (int c = 0; c < grid_data.GroupNames.size(); c++)
		{
			if (grid->Cells[c + 1][r + 1] == MISSING_DATA_SIGN.c_str() || grid->Cells[c + 1][r + 1] == "")
				grid_data.Values[c][r] = MISSING_DATA_NUM;
			else
				grid_data.Values[c][r] = _wtof(grid->Cells[c + 1][r + 1].c_str());
		}



	return;
}
// ---------------------------------------------------------------------------

bool TMainForm::SaveProjectToFile(string filename)
{
	Screen->Cursor = crHourGlass;
	FormUserInfo->MemoPad->Lines->Clear();
	FormUserInfo->MemoPad->Lines->Add("Saving to file:");
	FormUserInfo->MemoPad->Lines->Add(AnsiString(filename.c_str()));
    FormUserInfo->Show();
	ofstream ofile(filename.c_str());

	ofile << "##################################################" << endl;
	ofile << "#                                                #" << endl;
	ofile << "#          " << c_str(SOFTWARE_NAME);
	for (int i=0; i<50-2-10-SOFTWARE_NAME.Length(); i++)    // 50=width of line, 50-2=number of potential spaces
		ofile << " ";
	ofile << "#" << endl;
	ofile << "#          Version: " << c_str(SOFTWARE_VERSION);
	for (int i=0; i<50-2-19-SOFTWARE_VERSION.Length(); i++)
		ofile << " ";
	ofile << "#" << endl;
	ofile << "#                                                #" << endl;
	ofile << "#          Developed by:                         #" << endl;
	ofile << "#          WSC Scientific GmbH                   #" << endl;
	ofile << "#          Dossenheimer Landstr. 9/1             #" << endl;
	ofile << "#          69121 Heidelberg                      #" << endl;
	ofile << "#          Germany                               #" << endl;
	ofile << "#                                                #" << endl;
	ofile << "##################################################" << endl;
	ofile << "#                                                #" << endl;
	ofile << "#               GUTS project file                #" << endl;
	ofile << "#        do not modify this file manually        #" << endl;
	ofile << "#                                                #" << endl;
	ofile << "##################################################" << endl;
	ofile << "Project file version: " << c_str(PROJECT_FILE_VERSION) << endl;

	ofile << "##################################################" << endl;
	ofile << "###############   INFORMATION   ##################" << endl;
	ofile << "##################################################" << endl;
	ofile << "#-------------------------------------------------" << endl;
	ofile << "# Lines starting with '#' are comments" << endl;
	ofile << "# Missing data is represented by '-'" << endl;
	ofile << "# Columns are tab-seperated" << endl;
	ofile << "#-------------------------------------------------" << endl;

	// ProjectDescriptionDlg->ProjectDescriptionRichEdit; --> saved in ProjectDescription
	// PROJECT DESCRIPTION
	ofile << "##################################################" << endl;
	ofile << "#-------------------------------------------------" << endl;
	ofile << "# Project description:" << endl;
	ofile << "#-------------------------------------------------" << endl;
	ofile << ProjectDescription << endl;
	ofile << "#-------------------------------------------------" << endl;

	// CHECK and GET data before saving
	if (!CheckAndGetCalibrationData(CalibrationData))
	{
		Screen->Cursor = crDefault;
		return false;
	}

	ofile << "##################################################" << endl;
	ofile << "#############   CALIBRATION DATA   ###############" << endl;
	ofile << "##################################################" << endl;
	ofile << "#-------------------------------------------------" << endl;
	ofile << "Number of data sets:\t" << NumberOfCalibrationDataSets << endl;
	ofile << "#-------------------------------------------------" << endl;
	// for each calibration data set
	for (int d = 0; d < NumberOfCalibrationDataSets; d++)
	{
		// write description to file
		ofile << AnsiString(CalibDataDescriptionRichEdits[d]->Lines->Text);
		// write survival grid to file
		WriteGridDataToOStream(ofile, CalibSurvStringGrids[d]);
		// write concentration unit to file
		ofile << "Concentration unit:\t" << AnsiString(ConcUnitEdit->Text) << endl;
		// write concentration grid to file
		WriteGridDataToOStream(ofile, CalibConcStringGrids[d]);
		ofile << "#-------------------------------------------------" << endl;
	}

	ofile << "##################################################" << endl;
	ofile << "######   CALIBRATION SETTINGS / RESULTS   ########" << endl;
	ofile << "##################################################" << endl;
	ofile << "#-------------------------------------------------" << endl;
	ofile << "Calibration performed: " << (CalibrationPerformed ? "Yes" : "No") << endl;
	ofile << "Parameter ranges changed manually: " << (ParameterRangesManipulated ? "Yes" : "No") << endl;
	ofile << "Pre-fit background mortality to control: " << (PrefitHBToControl ? "Yes" : "No") << endl;
	ofile << "Expert settings: " << (ExpertSettingsOn ? "Yes" : "No") << endl;
/* the following is commented since it refers to leftovers from previous versions:
	ofile << "Use calculated parameter ranges based on input data (SD): " << (!ParameterRangesManipulated ? "Yes" :
		"No") << endl;
	ofile << "Use existing parameter sample (SD): " << (SDUseExistingParameterSample ? "Yes" : "No") << endl;
	ofile << "Use calculated parameter ranges based on input data (IT): " << (!ParameterRangesManipulated ? "Yes" :
		"No") << endl;
	ofile << "Use existing parameter sample (IT): " << (ITUseExistingParameterSample ? "Yes" : "No") << endl;*/
	ofile << "#-------------------------------------------------" << endl;
	// parameter matrix for GUTS SD:
	ofile << "# Parameter settings for GUTS-RED-SD:" << endl;
	ofile << "Parameter	fit y/n	Min	Max	norm/log" << endl;
	for (int p = 0; p < gutsPARAMETER_NO; p++)
		ofile << gutsPARAMETERS[p] << "\t" << PMatSD.Fit[p] << "\t" << PMatSD.Min[p] << "\t" << PMatSD.Max[p]
			<< "\t" << PMatSD.Scale[p] << endl;
	ofile << "#-------------------------------------------------" << endl;
	// parameter matrix for GUTS IT:
	ofile << "# Parameter settings for GUTS-RED-IT:" << endl;
	ofile << "Parameter	fit y/n	Min	Max	norm/log" << endl;
	for (int p = 0; p < gutsPARAMETER_NO; p++)
		ofile << gutsPARAMETERS[p] << "\t" << PMatIT.Fit[p] << "\t" << PMatIT.Min[p] << "\t" << PMatIT.Max[p]
			<< "\t" << PMatIT.Scale[p] << endl;
	ofile << "#-------------------------------------------------" << endl;
	// parameter results:
	if (CalibrationPerformed)
	{
		// for GUTS-RED-SD:
		ofile << "# Estimated parameters for GUTS-RED-SD:" << endl;
		ofile << "Parameter\tValue\tLower CI\tLower CI warning\tUpper CI\tUpper CI warning" << endl;
		for (int p = 0; p < gutsPARAMETER_NO; p++)
			ofile << gutsPARAMETERS[p] << "\t" << PMatPrintSD.BestValue[p] << "\t" << PMatPrintSD.LowerCI[p]
				<< "\t" << PMatPrintSD.LowerCIWarning[p] << "\t" << PMatPrintSD.UpperCI[p] << "\t" << PMatPrintSD.UpperCIWarning
				[p] << endl;
		ofile << "#-------------------------------------------------" << endl;

		// GUTS-RED-SD Goodness Of Fit:
		ofile << "# Goodness of fit for GUTS-RED-SD:" << endl;
		ofile << "Model efficiency (NSE, r-square): " << CalibrationSDGoodnessOfFit.R_square << endl;
		ofile << "Normalised root-means-square error (NRMSE): " << CalibrationSDGoodnessOfFit.NRMSE << " %" << endl;
		ofile << "Minus log-likelihood (MLL): " << CalibrationSDGoodnessOfFit.MLL << endl;
		ofile << "AIC: " << CalibrationSDGoodnessOfFit.AIC << endl;
		ofile << "Survival probability prediction error (SPPE) for each treatment:" << endl;
		ofile << "Data set\tTreatment\tValue" << endl;
		for (int i=0; i<CalibrationSDGoodnessOfFit.SPPE.size(); i++)
			ofile << CalibrationSDGoodnessOfFit.SPPE[i] << endl;
		ofile << "#-------------------------------------------------" << endl;

		// GUTS-RED-SD LCx:
		ofile << "# GUTS-RED-SD results table for LCx,t [[C]], with 95% CI:" << endl;
		for (int i=0; i<LCxSD_out.size(); i++)
			ofile << LCxSD_out[i] << endl;
		ofile << "#-------------------------------------------------" << endl;

		// for GUTS-RED-IT:
		ofile << "# Estimated parameters for GUTS-RED-IT:" << endl;
		ofile << "Parameter\tValue\tLower CI\tLower CI warning\tUpper CI\tUpper CI warning" << endl;
		for (int p = 0; p < gutsPARAMETER_NO; p++)
			ofile << gutsPARAMETERS[p] << "\t" << PMatPrintIT.BestValue[p] << "\t" << PMatPrintIT.LowerCI[p]
				<< "\t" << PMatPrintIT.LowerCIWarning[p] << "\t" << PMatPrintIT.UpperCI[p] << "\t" << PMatPrintIT.UpperCIWarning
				[p] << endl;
		ofile << "#-------------------------------------------------" << endl;

		// GUTS-RED-IT Goodness Of Fit:
		ofile << "# Goodness of fit for GUTS-RED-IT:" << endl;
		ofile << "Model efficiency (NSE, r-square): " << CalibrationITGoodnessOfFit.R_square << endl;
		ofile << "Normalised root-means-square error (NRMSE): " << CalibrationITGoodnessOfFit.NRMSE << " %" << endl;
		ofile << "Minus log-likelihood (MLL): " << CalibrationITGoodnessOfFit.MLL << endl;
		ofile << "AIC: " << CalibrationITGoodnessOfFit.AIC << endl;
		ofile << "Survival probability prediction error (SPPE) for each treatment:" << endl;
		ofile << "Data set\tTreatment\tValue" << endl;
		for (int i=0; i<CalibrationITGoodnessOfFit.SPPE.size(); i++)
			ofile << CalibrationITGoodnessOfFit.SPPE[i] << endl;
		ofile << "#-------------------------------------------------" << endl;

		// GUTS-RED-IT LCx:
		ofile << "# GUTS-RED-IT results table for LCx,t [[C]], with 95% CI:" << endl;
		for (int i=0; i<LCxIT_out.size(); i++)
			ofile << LCxIT_out[i] << endl;
		ofile << "#-------------------------------------------------" << endl;
	}

	ofile << "##################################################" << endl;
	ofile << "##############   VALIDATION DATA   ###############" << endl;
	ofile << "##################################################" << endl;
	ofile << "#-------------------------------------------------" << endl;
	ofile << "SD validation performed: " << (SDValidationPerformed ? "Yes" : "No") << endl;
	ofile << "IT validation performed: " << (ITValidationPerformed ? "Yes" : "No") << endl;
	ofile << "Pre-fit background mortality to control: " << (FitHBToValidationControl ? "Yes" : "No") << endl;
	ofile << "#-------------------------------------------------" << endl;
	// write description to file
	ofile << AnsiString(ValidationDataDescriptionRichEdit->Lines->Text);
	// write survival grid to file
	WriteGridDataToOStream(ofile, ValidationSurvStringGrid);
	// write concentration unit to file
	ofile << "Concentration unit:\t" << AnsiString(ConcUnitEdit->Text) << endl;
	// write concentration grid to file
	WriteGridDataToOStream(ofile, ValidationConcStringGrid);
	ofile << "#-------------------------------------------------" << endl;
	if (SDValidationPerformed)
	{
		// GUTS-RED-SD Goodness Of Fit:
		ofile << "# Goodness of fit for GUTS-RED-SD:" << endl;
		ofile << "Model efficiency (NSE, r-square): " << ValidationSDGoodnessOfFit.R_square << endl;
		ofile << "Normalised root-means-square error (NRMSE): " << ValidationSDGoodnessOfFit.NRMSE << " %" << endl;
		ofile << "Survival probability prediction error (SPPE) for each treatment:" << endl;
		ofile << "Data set\tTreatment\tValue" << endl;
		for (int i=0; i<ValidationSDGoodnessOfFit.SPPE.size(); i++)
			ofile << ValidationSDGoodnessOfFit.SPPE[i] << endl;
		ofile << "#-------------------------------------------------" << endl;
	}
	if (ITValidationPerformed)
	{
		// GUTS-RED-IT Goodness Of Fit:
		ofile << "# Goodness of fit for GUTS-RED-IT:" << endl;
		ofile << "Model efficiency (NSE, r-square): " << ValidationITGoodnessOfFit.R_square << endl;
		ofile << "Normalised root-means-square error (NRMSE): " << ValidationITGoodnessOfFit.NRMSE << " %" << endl;
		ofile << "Survival probability prediction error (SPPE) for each treatment:" << endl;
		ofile << "Data set\tTreatment\tValue" << endl;
		for (int i=0; i<ValidationITGoodnessOfFit.SPPE.size(); i++)
			ofile << ValidationITGoodnessOfFit.SPPE[i] << endl;
		ofile << "#-------------------------------------------------" << endl;
	}

	ofile << "##################################################" << endl;
	ofile << "##############   PREDICTIONS DATA   ##############" << endl;
	ofile << "##################################################" << endl;
	ofile << "#-------------------------------------------------" << endl;
	// write concentration grid to file
	WriteGridDataToOStream(ofile, PredictionsConcStringGrid);

	ofile << "##################################################" << endl;
	ofile << "############   BATCH PREDICTIONS DATA   ##########" << endl;
	ofile << "##################################################" << endl;
	ofile << "#-------------------------------------------------" << endl;

	// write LPx results to file
	if (LPxBatch_out.size() > 0) {
		if (LPxBatch_Samplesize_Info_out.size() > 0) {
			for (
				vector<AnsiString >::const_iterator cit = LPxBatch_Samplesize_Info_out.begin();
				cit != LPxBatch_Samplesize_Info_out.end();
				++cit
			)
			{
				ofile << *cit << endl;
			}
			ofile << endl;
		}

		for (int i = 0; i < LPxBatch_out.size(); i++) {
			ofile << LPxBatch_out[i] << endl;
		}
	}
	else
	{
		ofile << "No batch prediction performed." << endl;
    }

	ofile << "#-------------------------------------------------" << endl;

	FormUserInfo->MemoPad->Lines->Add("");
	FormUserInfo->MemoPad->Lines->Add("Saving calibrated parameter space.");
	FormUserInfo->MemoPad->Lines->Add("Please wait. This might take a moment ...");
	FormUserInfo->MemoPad->Lines->Add("  Starting with SD");
	ofile << "##################################################" << endl;
	ofile << "##############   PARAMETER SPACES   ##############" << endl;
	ofile << "##################################################" << endl;
	ofile << "#-------------------------------------------------" << endl;
	ofile << "Number of parameter sets for SD:\t" << CalibrationParameterSpaceSD.size() << endl;
	ofile << "#-------------------------------------------------" << endl;
	ofile << "Parameter set\tkd\tmw\thb\tbw\tFs\tlog-likelihood" << endl;
	for (int s = 0; s < CalibrationParameterSpaceSD.size(); s++)
	{
		Application->ProcessMessages();
		ofile << s + 1;
		for (int p = 0; p < gutsPARAMETER_NO; p++)
			ofile << "\t" << CalibrationParameterSpaceSD[s].Parameter[p];
		ofile << "\t" << CalibrationParameterSpaceSD[s].Likelihood << endl;
	}
	FormUserInfo->MemoPad->Lines->Add("  Parameter space saved for SD");
	FormUserInfo->MemoPad->Lines->Add("  Starting with IT");
	ofile << "#-------------------------------------------------" << endl;
	ofile << "Number of parameter sets for IT:\t" << CalibrationParameterSpaceIT.size() << endl;
	ofile << "#-------------------------------------------------" << endl;
	ofile << "Parameter set\tkd\tmw\thb\tbw\tFs\tlog-likelihood" << endl;
	for (int s = 0; s < CalibrationParameterSpaceIT.size(); s++)
	{
        Application->ProcessMessages();
		ofile << s + 1;
		for (int p = 0; p < gutsPARAMETER_NO; p++)
			ofile << "\t" << CalibrationParameterSpaceIT[s].Parameter[p];
		ofile << "\t" << CalibrationParameterSpaceIT[s].Likelihood << endl;
	}
	ofile << "#-------------------------------------------------" << endl;
	FormUserInfo->MemoPad->Lines->Add("  Parameter space saved for IT");
	ofile << "##################################################" << endl;
	ofile << "################   END OF FILE   #################" << endl;
	ofile << "##################################################" << endl;

	ofile.close();
	Screen->Cursor = crArrow;
	if (!ofile)
	{
        FormUserInfo->MemoPad->Lines->Clear();
		FormUserInfo->Hide();
		ShowMessage("Error while saving file");
		return false;
	}

	ProjectSaved = true;
    FormUserInfo->MemoPad->Lines->Add("");
	FormUserInfo->MemoPad->Lines->Add("Saving project completed.");
	Sleep(2000);
	FormUserInfo->MemoPad->Lines->Clear();
	FormUserInfo->Hide();
	return true;
}
// ---------------------------------------------------------------------------

bool TMainForm::OpenProjectFromFile(string filename)
{
	if (filename == "")
		return false;

	string s;
	vector<string>sv;
	sv = SplitString(filename, ".");
	if (sv.size() > 0 && sv[sv.size()-1] != AnsiString(PROJECT_FILE_EXT).c_str())
	{
		ShowMessage("Please provide an " + AnsiString(SOFTWARE_NAME) +" project file (." + AnsiString(PROJECT_FILE_EXT) + ").");
		return false;
	}

	Screen->Cursor = crHourGlass;
	ifstream ifile;
	if (ifile.is_open())
		ifile.close();
	ifile.open(filename.c_str());


	// #####################   PROJECT FILE VERSION   #####################
	GetNoCommentLine(ifile, s); // "Project file version: "
	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	ProjectFileVersion = sv[sv.size() - 1];

	if (ProjectFileVersion.compare("1.0") == 0)
	{
		if (!OpenProjectFromFile_v1_0(ifile))
		{
			Screen->Cursor = crDefault;
			return false;
		}
	}
	else if (ProjectFileVersion.compare("1.1") == 0)
	{
		if (!OpenProjectFromFile_v1_1(ifile))
		{
			Screen->Cursor = crDefault;
			return false;
		}
	}
	else if (ProjectFileVersion.compare("1.2") == 0)
	{
		if (!OpenProjectFromFile_v1_2(ifile))
		{
			Screen->Cursor = crDefault;
			return false;
		}
	}
	else if (ProjectFileVersion.compare("1.3") == 0)
	{
		if (!OpenProjectFromFile_v1_3(ifile))
		{
			Screen->Cursor = crDefault;
			return false;
		}
	}
	else if (ProjectFileVersion.compare("1.4") == 0)
	{
		if (!OpenProjectFromFile_v1_4(ifile))
		{
			Screen->Cursor = crDefault;
			return false;
		}
	}
	else
	{
		Screen->Cursor = crDefault;
		return false;
	}

	Screen->Cursor = crDefault;
	return true;
}
// ---------------------------------------------------------------------------

void TMainForm::WriteConcSurvCheckedDataToGUI(TGridData &data, TStringGrid *grid, int data_type)
{
	// prepare grid size
	grid->RowCount = data.Times.size() + 10;
	grid->ColCount = data.GroupNames.size() + 10;

	if (data_type == gutsSURV_DATA)
		grid->Cells[0][0] = "Surv. time [d]";
	else
		grid->Cells[0][0] = "Conc. time [d]";

	for (int c=0; c<data.GroupNames.size(); c++)
		grid->Cells[c+1][0] = data.GroupNames[c];
	for (int r=0; r<data.Times.size(); r++)
		grid->Cells[0][r+1] = data.Times[r];
	for (int c=0; c<data.GroupNames.size(); c++)
		for (int r=0; r<data.Times.size(); r++)
			if (data.Values[c][r] != MISSING_DATA_NUM)
				grid->Cells[c+1][r+1] = data.Values[c][r];

	return;
}
// ---------------------------------------------------------------------------
bool TMainForm::OpenProjectFromFile_v1_4(ifstream &ifile)
{
	string s;
	vector<string> sv;

	try {
		// PROJECT DESCRIPTION
		GetNoCommentLine(ifile, s);
		ProjectDescription = s;
		getline(ifile, s);
		while (s.length() == 0 || (s.length() > 0 && s.substr(0, 1) != "#"))
			ProjectDescription += "\r\n" + s;
		ProjectDescriptionDlg->ProjectDescriptionRichEdit->Text = ProjectDescription.c_str();

		// #####################   CALIBRATION DATA   #####################
		GetNoCommentLine(ifile, s); // "Number of data sets:\t"
		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		int num_calib_sets = AnsiString(sv[sv.size() - 1].c_str()).ToInt();
		if (num_calib_sets <= MAX_CALIBRATION_DATA_SETS_NO)
			InputDataCalibSetNoSpin->Value = num_calib_sets;
		else
			return false;
		for (int d = 0; d < num_calib_sets; d++)
		{
			if (!WriteConcSurvInputFileToGUI(ifile, CalibDataDescriptionRichEdits[d], CalibSurvStringGrids[d],
					CalibConcStringGrids[d], ConcUnitEdit, CalibControlCombos[d]))
				return false;
		}
		if (!CheckAndGetCalibrationData(CalibrationData))
			return false;

		// #####################   CALIBRATION SETTINGS/RESULTS   #####################
		GetNoCommentLine(ifile, s); // "Calibration performed: "
		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		if (sv[sv.size() - 1] == "Yes")
			CalibrationPerformed = true;
		else if (sv[sv.size() - 1] == "No")
			CalibrationPerformed = false;
		else
			return false;

		GetNoCommentLine(ifile, s); // "Parameter ranges changed manually: "
		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		if (sv[sv.size() - 1] == "Yes")
			ParameterRangesManipulated = true;
		else if (sv[sv.size() - 1] == "No")
			ParameterRangesManipulated = false;
		else
			return false;

		GetNoCommentLine(ifile, s); // "Pre-fit background mortality to control: "
		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		if (sv[sv.size() - 1] == "Yes")
			PrefitHBToControl = true;
		else if (sv[sv.size() - 1] == "No")
			PrefitHBToControl = false;
		else
			return false;
		// now prepare the calibration data sets after the bg mortality info for e.g. another calibration
		Prepare_Data(CalibData_All, CalibData_HB, CalibrationData,
				NumberOfCalibrationDataSets, PrefitHBToControl);

		GetNoCommentLine(ifile, s); // "Expert settings: "
		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		if (sv[sv.size() - 1] == "Yes")
			ExpertSettingsOn = true;
		else if (sv[sv.size() - 1] == "No")
			ExpertSettingsOn = false;
		else
			return false;


		GetNoCommentLine(ifile, s); // "Parameter	fit y/n	Min	Max	norm/log"

		for (int p = 0; p < gutsPARAMETER_NO; p++)
		{
			GetNoCommentLine(ifile, s);
			sv = SplitString(s, "\t");
			if (sv[1] != MISSING_DATA_SIGN)
				PMatSD.Fit[p] = AnsiString(sv[1].c_str()).ToInt();
			else
				PMatSD.Fit[p] = -1;
			if (sv[2] != MISSING_DATA_SIGN)
				PMatSD.Min[p] = AnsiString(sv[2].c_str()).ToDouble();
			else
				PMatSD.Min[p] = -1;
			if (sv[3] != MISSING_DATA_SIGN)

				PMatSD.Max[p] = AnsiString(sv[3].c_str()).ToDouble();
			else
				PMatSD.Max[p] = -1;
			if (sv[4] != MISSING_DATA_SIGN)
				PMatSD.Scale[p] = AnsiString(sv[4].c_str()).ToInt();
			else
				PMatSD.Scale[p] = -1;
		}
		GetNoCommentLine(ifile, s); // "Parameter	fit y/n	Min	Max	norm/log"
		for (int p = 0; p < gutsPARAMETER_NO; p++)
		{
			GetNoCommentLine(ifile, s);
			sv = SplitString(s, "\t");
			if (sv[1] != MISSING_DATA_SIGN)
				PMatIT.Fit[p] = AnsiString(sv[1].c_str()).ToInt();
			else
				PMatIT.Fit[p] = -1;
			if (sv[2] != MISSING_DATA_SIGN)
				PMatIT.Min[p] = AnsiString(sv[2].c_str()).ToDouble();
			else
				PMatIT.Min[p] = -1;
			if (sv[3] != MISSING_DATA_SIGN)
				PMatIT.Max[p] = AnsiString(sv[3].c_str()).ToDouble();
			else
				PMatIT.Max[p] = -1;
			if (sv[4] != MISSING_DATA_SIGN)
				PMatIT.Scale[p] = AnsiString(sv[4].c_str()).ToInt();
			else
				PMatIT.Scale[p] = -1;
		}
		if (CalibrationPerformed)
		{
			// SD results
			PMatPrintSD.BestValue.resize(gutsPARAMETER_NO);
			PMatPrintSD.LowerCI.resize(gutsPARAMETER_NO);
			PMatPrintSD.LowerCIWarning.resize(gutsPARAMETER_NO);
			PMatPrintSD.UpperCI.resize(gutsPARAMETER_NO);
			PMatPrintSD.UpperCIWarning.resize(gutsPARAMETER_NO);
			PMatPrintSD.LowerEdgeJoint95.resize(gutsPARAMETER_NO);
			PMatPrintSD.UpperEdgeJoint95.resize(gutsPARAMETER_NO);
			PMatPrintSD.Broken.resize(gutsPARAMETER_NO);
			GetNoCommentLine(ifile, s);		// "Parameter\tValue\tLower CI\tLower CI warning\tUpper CI\tUpper CI warning"
			for (int p = 0; p < gutsPARAMETER_NO; p++)
			{
				GetNoCommentLine(ifile, s);
				sv = SplitString(s, "\t");
				if (sv[1] != MISSING_DATA_SIGN)
					PMatPrintSD.BestValue[p] = AnsiString(sv[1].c_str()).ToDouble();
				else
					PMatPrintSD.BestValue[p] = -1;
				if (sv[2] != MISSING_DATA_SIGN)
					PMatPrintSD.LowerCI[p] = AnsiString(sv[2].c_str()).ToDouble();
				else
					PMatPrintSD.LowerCI[p] = -1;
				if (sv[3] != MISSING_DATA_SIGN)
					PMatPrintSD.LowerCIWarning[p] = sv[3];
				else
					PMatPrintSD.LowerCIWarning[p] = "-";
				if (sv[4] != MISSING_DATA_SIGN)
					PMatPrintSD.UpperCI[p] = AnsiString(sv[4].c_str()).ToDouble();
				else
					PMatPrintSD.UpperCI[p] = -1;
				if (sv[5] != MISSING_DATA_SIGN)
					PMatPrintSD.UpperCIWarning[p] = sv[5];
				else
					PMatPrintSD.UpperCIWarning[p] = "-";
			}

			// SD Goodness of fit
			GetNoCommentLine(ifile, s);		// "Model efficiency (NSE, r-square):"
			sv = SplitString(s, ":");
			EraseSpaces(sv[sv.size() - 1]);
			CalibrationSDGoodnessOfFit.R_square = AnsiString(sv[sv.size() - 1].c_str());

			GetNoCommentLine(ifile, s);		// "Normalised root-means-square error (NRMSE):"
			sv = SplitString(s, ":");
			EraseSpaces(sv[sv.size() - 1]);
			CalibrationSDGoodnessOfFit.NRMSE = AnsiString(sv[sv.size() - 1].c_str());

			GetNoCommentLine(ifile, s);		// "Minus log-likelihood (MLL):"
			sv = SplitString(s, ":");
			EraseSpaces(sv[sv.size() - 1]);
			CalibrationSDGoodnessOfFit.MLL = AnsiString(sv[sv.size() - 1].c_str());

			GetNoCommentLine(ifile, s);		// "AIC:"
			sv = SplitString(s, ":");
			EraseSpaces(sv[sv.size() - 1]);
			CalibrationSDGoodnessOfFit.AIC = AnsiString(sv[sv.size() - 1].c_str());

			GetNoCommentLine(ifile, s);		// "Survival probability prediction error (SPPE) for each treatment:"
			GetNoCommentLine(ifile, s);		// "Data set	Treatment	Value"
			CalibrationSDGoodnessOfFit.SPPE.resize(0);
			getline(ifile, s);              // get first line
			while (s.substr(0, 1) != "#")
			{
				CalibrationSDGoodnessOfFit.SPPE.push_back(AnsiString(s.c_str()));
				getline(ifile, s);
			}

			// SD LCx
			LCxSD_out.resize(0);
			GetNoCommentLine(ifile, s);
			while (s.substr(0, 1) != "#")
			{
				LCxSD_out.push_back(AnsiString(s.c_str()));
				getline(ifile, s);
			}

			// IT results
			PMatPrintIT.BestValue.resize(gutsPARAMETER_NO);
			PMatPrintIT.LowerCI.resize(gutsPARAMETER_NO);
			PMatPrintIT.LowerCIWarning.resize(gutsPARAMETER_NO);
			PMatPrintIT.UpperCI.resize(gutsPARAMETER_NO);
			PMatPrintIT.UpperCIWarning.resize(gutsPARAMETER_NO);
			PMatPrintIT.LowerEdgeJoint95.resize(gutsPARAMETER_NO);
			PMatPrintIT.UpperEdgeJoint95.resize(gutsPARAMETER_NO);
			PMatPrintIT.Broken.resize(gutsPARAMETER_NO);
			GetNoCommentLine(ifile, s);		// "Parameter\tValue\tLower CI\tLower CI warning\tUpper CI\tUpper CI warning"
			for (int p = 0; p < gutsPARAMETER_NO; p++)
			{
				GetNoCommentLine(ifile, s);
				sv = SplitString(s, "\t");
				if (sv[1] != MISSING_DATA_SIGN)
					PMatPrintIT.BestValue[p] = AnsiString(sv[1].c_str()).ToDouble();
				else
					PMatPrintIT.BestValue[p] = -1;
				if (sv[2] != MISSING_DATA_SIGN)
					PMatPrintIT.LowerCI[p] = AnsiString(sv[2].c_str()).ToDouble();
				else
					PMatPrintIT.LowerCI[p] = -1;
				if (sv[3] != MISSING_DATA_SIGN)
					PMatPrintIT.LowerCIWarning[p] = sv[3];
				else
					PMatPrintIT.LowerCIWarning[p] = "-";
				if (sv[4] != MISSING_DATA_SIGN)
					PMatPrintIT.UpperCI[p] = AnsiString(sv[4].c_str()).ToDouble();
				else
					PMatPrintIT.UpperCI[p] = -1;
				if (sv[5] != MISSING_DATA_SIGN)
					PMatPrintIT.UpperCIWarning[p] = sv[5];
				else
					PMatPrintIT.UpperCIWarning[p] = "-";
			}

			// IT Goodness of fit
			GetNoCommentLine(ifile, s);		// "Model efficiency (NSE, r-square):"
			sv = SplitString(s, ":");
			EraseSpaces(sv[sv.size() - 1]);
			CalibrationITGoodnessOfFit.R_square = AnsiString(sv[sv.size() - 1].c_str());

			GetNoCommentLine(ifile, s);		// "Normalised root-means-square error (NRMSE):"
			sv = SplitString(s, ":");
			EraseSpaces(sv[sv.size() - 1]);
			CalibrationITGoodnessOfFit.NRMSE = AnsiString(sv[sv.size() - 1].c_str());

			GetNoCommentLine(ifile, s);		// "Minus log-likelihood (MLL):"
			sv = SplitString(s, ":");
			EraseSpaces(sv[sv.size() - 1]);
			CalibrationITGoodnessOfFit.MLL = AnsiString(sv[sv.size() - 1].c_str());

			GetNoCommentLine(ifile, s);		// "AIC:"
			sv = SplitString(s, ":");
			EraseSpaces(sv[sv.size() - 1]);
			CalibrationITGoodnessOfFit.AIC = AnsiString(sv[sv.size() - 1].c_str());

			GetNoCommentLine(ifile, s);		// "Survival probability prediction error (SPPE) for each treatment:"
			GetNoCommentLine(ifile, s);		// "Data set	Treatment	Value"
			CalibrationITGoodnessOfFit.SPPE.resize(0);
			getline(ifile, s);      		// get first line
			while (s.substr(0, 1) != "#")
			{
				CalibrationITGoodnessOfFit.SPPE.push_back(AnsiString(s.c_str()));
				getline(ifile, s);
			}

			// IT LCx
			LCxIT_out.resize(0);
			GetNoCommentLine(ifile, s);
			while (s.substr(0, 1) != "#")
			{
				LCxIT_out.push_back(AnsiString(s.c_str()));
				getline(ifile, s);
			}

			// adjust values of PMatSD and PMatIT if possible
			PMatSD.Value = PMatPrintSD.BestValue;
			PMatIT.Value = PMatPrintIT.BestValue;
		}
		else
		{
			PMatSD.Value = PMatSD.Max;
			PMatIT.Value = PMatIT.Max;
		}

		// display the parameters in the GUI
		bool change_value = true;
		bool change_range = true;
		bool remember_calibration_performed = CalibrationPerformed;
		DisplayParameterMatrixInGUI(PMatSD, gutsSD, change_value, change_range);
		DisplayParameterMatrixInGUI(PMatIT, gutsIT, change_value, change_range);
		CalibrationPerformed = remember_calibration_performed;

		// ################ VALIDATION DATA ###################
		GetNoCommentLine(ifile, s); // "SD validation performed: "
		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		if (sv[sv.size() - 1] == "Yes")
			SDValidationPerformed = true;
		else if (sv[sv.size() - 1] == "No")
			SDValidationPerformed = false;
		else
			return false;

		GetNoCommentLine(ifile, s); // "IT validation performed: "
		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		if (sv[sv.size() - 1] == "Yes")
			ITValidationPerformed = true;
		else if (sv[sv.size() - 1] == "No")
			ITValidationPerformed = false;
		else
			return false;

		GetNoCommentLine(ifile, s); // "Pre-fit background mortality to control: "
		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		if (sv[sv.size() - 1] == "Yes")
			FitHBToValidationControl = true;
		else if (sv[sv.size() - 1] == "No")
			FitHBToValidationControl = false;
		else
			return false;

		ValidationDataPresent = false;
		if (!WriteConcSurvInputFileToGUI(ifile, ValidationDataDescriptionRichEdit, ValidationSurvStringGrid,
				ValidationConcStringGrid, ConcUnitEdit, ValidControlCombo))
			return false;
		if (ValidationDataPresent && (SDValidationPerformed || ITValidationPerformed))
			if (!CheckAndGetValidationData(ValidationData))
				return false;
		if (SDValidationPerformed)
		{
			// SD Goodness of fit
			GetNoCommentLine(ifile, s);		// "Model efficiency (NSE, r-square):"
			sv = SplitString(s, ":");
			EraseSpaces(sv[sv.size() - 1]);
			ValidationSDGoodnessOfFit.R_square = AnsiString(sv[sv.size() - 1].c_str());

			GetNoCommentLine(ifile, s);		// "Normalised root-means-square error (NRMSE):"
			sv = SplitString(s, ":");
			EraseSpaces(sv[sv.size() - 1]);
			ValidationSDGoodnessOfFit.NRMSE = AnsiString(sv[sv.size() - 1].c_str());

			GetNoCommentLine(ifile, s);		// "Survival probability prediction error (SPPE) for each treatment:"
			GetNoCommentLine(ifile, s);		// "Data set	Treatment	Value"
			ValidationSDGoodnessOfFit.SPPE.resize(0);
			getline(ifile, s);      		// get first line
			while (s.substr(0, 1) != "#")
			{
				ValidationSDGoodnessOfFit.SPPE.push_back(AnsiString(s.c_str()));
				getline(ifile, s);
			}
		}
		if (ITValidationPerformed)
		{
			// SD Goodness of fit
			GetNoCommentLine(ifile, s);		// "Model efficiency (NSE, r-square):"
			sv = SplitString(s, ":");
			EraseSpaces(sv[sv.size() - 1]);
			ValidationITGoodnessOfFit.R_square = AnsiString(sv[sv.size() - 1].c_str());

			GetNoCommentLine(ifile, s);		// "Normalised root-means-square error (NRMSE):"
			sv = SplitString(s, ":");
			EraseSpaces(sv[sv.size() - 1]);
			ValidationITGoodnessOfFit.NRMSE = AnsiString(sv[sv.size() - 1].c_str());

			GetNoCommentLine(ifile, s);		// "Survival probability prediction error (SPPE) for each treatment:"
			GetNoCommentLine(ifile, s);		// "Data set	Treatment	Value"
			ValidationITGoodnessOfFit.SPPE.resize(0);
			getline(ifile, s);          	// get first line
			while (s.substr(0, 1) != "#")
			{
				ValidationITGoodnessOfFit.SPPE.push_back(AnsiString(s.c_str()));
				getline(ifile, s);
			}
		}

		// ################ PREDICTIONS DATA ###################
		PredictionsDataPresent = false;
		if (!WritePredictionsConcDataToGUI(ifile, PredictionsConcStringGrid))
			return false;
		if (PredictionsDataPresent)
		{
			GetDataFromGrid(PredictionsConcStringGrid, SingleExposureProfile);

			// build data structure
			PredictionsData.resize(1);
			PredictionsData[0].Concentration = SingleExposureProfile;

			// add dummy survival data as proposed by Tjalling Jager
			PredictionsData[0].Survival.Times.resize(2);
			PredictionsData[0].Survival.Times[0] = 0;
			PredictionsData[0].Survival.Times[1] = SingleExposureProfile.Times[SingleExposureProfile.Times.size() - 1];
		}

		// ################ BATCH PREDICTIONS DATA ###############
		if (!WriteBatchPredictionsConcDataToGUI(ifile)) return false;

		// ############## PARAMETER SPACES ###################
		GetNoCommentLine(ifile, s); // "Number of parameter sets for SD:\t"
		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		NumberOfParameterSamplesSD = AnsiString(sv[sv.size() - 1].c_str()).ToInt();
		GetNoCommentLine(ifile, s);
		// "Parameter set\tkd\tmw\thb\tbw\tFs\tlog-likelihood"
		CalibrationParameterSpaceSD.resize(0);
		for (int ns = 0; ns < NumberOfParameterSamplesSD; ns++)
		{
			GetNoCommentLine(ifile, s);
			sv = SplitString(s, "\t");
			vector<double>param_temp;
			param_temp.resize(0);
			for (int p = 1; p < sv.size() - 1; p++)
				param_temp.push_back(AnsiString(sv[p].c_str()).ToDouble());
			CalibrationParameterSpaceSD.resize(CalibrationParameterSpaceSD.size() + 1);
			CalibrationParameterSpaceSD[CalibrationParameterSpaceSD.size() - 1].Parameter = param_temp;
			CalibrationParameterSpaceSD[CalibrationParameterSpaceSD.size() - 1].Likelihood =
					AnsiString(sv[sv.size() - 1].c_str()).ToDouble();
		}
		GetNoCommentLine(ifile, s); // "Number of parameter sets for IT:\t"
		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		NumberOfParameterSamplesIT = AnsiString(sv[sv.size() - 1].c_str()).ToInt();
		GetNoCommentLine(ifile, s);
		// "Parameter set\tkd\tmw\thb\tbw\tFs\tlog-likelihood"
		CalibrationParameterSpaceIT.resize(0);
		for (int ns = 0; ns < NumberOfParameterSamplesIT; ns++)
		{
			GetNoCommentLine(ifile, s);
			sv = SplitString(s, "\t");
			vector<double>param_temp;
			param_temp.resize(0);
			for (int p = 1; p < sv.size() - 1; p++)
				param_temp.push_back(AnsiString(sv[p].c_str()).ToDouble());
			CalibrationParameterSpaceIT.resize(CalibrationParameterSpaceIT.size() + 1);
			CalibrationParameterSpaceIT[CalibrationParameterSpaceIT.size() - 1].Parameter = param_temp;
			CalibrationParameterSpaceIT[CalibrationParameterSpaceIT.size() - 1].Likelihood =
					AnsiString(sv[sv.size() - 1].c_str()).ToDouble();
		}

		ifile.close();
		Screen->Cursor = crDefault;
	}
	catch (...) {
		Screen->Cursor = crDefault;
		return false;
	}

	return true;
}
// ---------------------------------------------------------------------------

bool TMainForm::OpenProjectFromFile_v1_3(ifstream &ifile)
{
	string s;
	vector<string> sv;

	try {
		// PROJECT DESCRIPTION
		GetNoCommentLine(ifile, s);
		ProjectDescription = s;
		getline(ifile, s);
		while (s.length() == 0 || (s.length() > 0 && s.substr(0, 1) != "#"))
			ProjectDescription += "\r\n" + s;
		ProjectDescriptionDlg->ProjectDescriptionRichEdit->Text = ProjectDescription.c_str();

		// #####################   CALIBRATION DATA   #####################
		GetNoCommentLine(ifile, s); // "Number of data sets:\t"
		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		int num_calib_sets = AnsiString(sv[sv.size() - 1].c_str()).ToInt();
		if (num_calib_sets <= MAX_CALIBRATION_DATA_SETS_NO)
			InputDataCalibSetNoSpin->Value = num_calib_sets;
		else
			return false;
		for (int d = 0; d < num_calib_sets; d++)
		{
			if (!WriteConcSurvInputFileToGUI(ifile, CalibDataDescriptionRichEdits[d], CalibSurvStringGrids[d],
					CalibConcStringGrids[d], ConcUnitEdit, CalibControlCombos[d]))
				return false;
		}
		if (!CheckAndGetCalibrationData(CalibrationData))
			return false;

		// #####################   CALIBRATION SETTINGS/RESULTS   #####################
		GetNoCommentLine(ifile, s); // "Calibration performed: "
		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		if (sv[sv.size() - 1] == "Yes")
			CalibrationPerformed = true;
		else if (sv[sv.size() - 1] == "No")
			CalibrationPerformed = false;
		else
			return false;

		GetNoCommentLine(ifile, s); // "Parameter ranges changed manually: "
		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		if (sv[sv.size() - 1] == "Yes")
			ParameterRangesManipulated = true;
		else if (sv[sv.size() - 1] == "No")
			ParameterRangesManipulated = false;
		else
			return false;

		GetNoCommentLine(ifile, s); // "Pre-fit background mortality to control: "
		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		if (sv[sv.size() - 1] == "Yes")
			PrefitHBToControl = true;
		else if (sv[sv.size() - 1] == "No")
			PrefitHBToControl = false;
		else
			return false;
		// now prepare the calibration data sets after the bg mortality info for e.g. another calibration
		Prepare_Data(CalibData_All, CalibData_HB, CalibrationData,
				NumberOfCalibrationDataSets, PrefitHBToControl);

		GetNoCommentLine(ifile, s); // "Expert settings: "
		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		if (sv[sv.size() - 1] == "Yes")
			ExpertSettingsOn = true;
		else if (sv[sv.size() - 1] == "No")
			ExpertSettingsOn = false;
		else
			return false;

	/* the following is commented since it refers to leftovers from previous versions:
		GetNoCommentLine(ifile, s); // "Use calculated parameter ranges (SD): "

		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		if (sv[sv.size() - 1] == "Yes")
			SDUseCalculatedParameterRangesForCalib = true;
		else if (sv[sv.size() - 1] == "No")
			SDUseCalculatedParameterRangesForCalib = false;
		else
			return false;

		GetNoCommentLine(ifile, s); // "Use existing parameter sample (SD): "

		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		if (sv[sv.size() - 1] == "Yes")
			SDUseExistingParameterSample = true;
		else if (sv[sv.size() - 1] == "No")
			SDUseExistingParameterSample = false;
		else
			return false;

		GetNoCommentLine(ifile, s); // "Use calculated parameter ranges (IT): "

		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		if (sv[sv.size() - 1] == "Yes")
			ITUseCalculatedParameterRangesForCalib = true;
		else if (sv[sv.size() - 1] == "No")
			ITUseCalculatedParameterRangesForCalib = false;
		else
			return false;

		GetNoCommentLine(ifile, s); // "Use existing parameter sample (IT): "

		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		if (sv[sv.size() - 1] == "Yes")
			ITUseExistingParameterSample = true;
		else if (sv[sv.size() - 1] == "No")
			ITUseExistingParameterSample = false;
		else
			return false;                                             */

		GetNoCommentLine(ifile, s); // "Parameter	fit y/n	Min	Max	norm/log"

		for (int p = 0; p < gutsPARAMETER_NO; p++)
		{
			GetNoCommentLine(ifile, s);
			sv = SplitString(s, "\t");
			if (sv[1] != MISSING_DATA_SIGN)
				PMatSD.Fit[p] = AnsiString(sv[1].c_str()).ToInt();
			else
				PMatSD.Fit[p] = -1;
			if (sv[2] != MISSING_DATA_SIGN)
				PMatSD.Min[p] = AnsiString(sv[2].c_str()).ToDouble();
			else
				PMatSD.Min[p] = -1;
			if (sv[3] != MISSING_DATA_SIGN)

				PMatSD.Max[p] = AnsiString(sv[3].c_str()).ToDouble();
			else
				PMatSD.Max[p] = -1;
			if (sv[4] != MISSING_DATA_SIGN)
				PMatSD.Scale[p] = AnsiString(sv[4].c_str()).ToInt();
			else
				PMatSD.Scale[p] = -1;
		}
		GetNoCommentLine(ifile, s); // "Parameter	fit y/n	Min	Max	norm/log"
		for (int p = 0; p < gutsPARAMETER_NO; p++)
		{
			GetNoCommentLine(ifile, s);
			sv = SplitString(s, "\t");
			if (sv[1] != MISSING_DATA_SIGN)
				PMatIT.Fit[p] = AnsiString(sv[1].c_str()).ToInt();
			else
				PMatIT.Fit[p] = -1;
			if (sv[2] != MISSING_DATA_SIGN)
				PMatIT.Min[p] = AnsiString(sv[2].c_str()).ToDouble();
			else
				PMatIT.Min[p] = -1;
			if (sv[3] != MISSING_DATA_SIGN)
				PMatIT.Max[p] = AnsiString(sv[3].c_str()).ToDouble();
			else
				PMatIT.Max[p] = -1;
			if (sv[4] != MISSING_DATA_SIGN)
				PMatIT.Scale[p] = AnsiString(sv[4].c_str()).ToInt();
			else
				PMatIT.Scale[p] = -1;
		}
		if (CalibrationPerformed)
		{
			// SD results
			PMatPrintSD.BestValue.resize(gutsPARAMETER_NO);
			PMatPrintSD.LowerCI.resize(gutsPARAMETER_NO);
			PMatPrintSD.LowerCIWarning.resize(gutsPARAMETER_NO);
			PMatPrintSD.UpperCI.resize(gutsPARAMETER_NO);
			PMatPrintSD.UpperCIWarning.resize(gutsPARAMETER_NO);
			PMatPrintSD.LowerEdgeJoint95.resize(gutsPARAMETER_NO);
			PMatPrintSD.UpperEdgeJoint95.resize(gutsPARAMETER_NO);
			PMatPrintSD.Broken.resize(gutsPARAMETER_NO);
			GetNoCommentLine(ifile, s);		// "Parameter\tValue\tLower CI\tLower CI warning\tUpper CI\tUpper CI warning"
			for (int p = 0; p < gutsPARAMETER_NO; p++)
			{
				GetNoCommentLine(ifile, s);
				sv = SplitString(s, "\t");
				if (sv[1] != MISSING_DATA_SIGN)
					PMatPrintSD.BestValue[p] = AnsiString(sv[1].c_str()).ToDouble();
				else
					PMatPrintSD.BestValue[p] = -1;
				if (sv[2] != MISSING_DATA_SIGN)
					PMatPrintSD.LowerCI[p] = AnsiString(sv[2].c_str()).ToDouble();
				else
					PMatPrintSD.LowerCI[p] = -1;
				if (sv[3] != MISSING_DATA_SIGN)
					PMatPrintSD.LowerCIWarning[p] = sv[3];
				else
					PMatPrintSD.LowerCIWarning[p] = "-";
				if (sv[4] != MISSING_DATA_SIGN)
					PMatPrintSD.UpperCI[p] = AnsiString(sv[4].c_str()).ToDouble();
				else
					PMatPrintSD.UpperCI[p] = -1;
				if (sv[5] != MISSING_DATA_SIGN)
					PMatPrintSD.UpperCIWarning[p] = sv[5];
				else
					PMatPrintSD.UpperCIWarning[p] = "-";
			}

			// SD Goodness of fit
			GetNoCommentLine(ifile, s);		// "Model efficiency (NSE, r-square):"
			sv = SplitString(s, ":");
			EraseSpaces(sv[sv.size() - 1]);
			CalibrationSDGoodnessOfFit.R_square = AnsiString(sv[sv.size() - 1].c_str());

			GetNoCommentLine(ifile, s);		// "Normalised root-means-square error (NRMSE):"
			sv = SplitString(s, ":");
			EraseSpaces(sv[sv.size() - 1]);
			CalibrationSDGoodnessOfFit.NRMSE = AnsiString(sv[sv.size() - 1].c_str());

			GetNoCommentLine(ifile, s);		// "Minus log-likelihood (MLL):"
			sv = SplitString(s, ":");
			EraseSpaces(sv[sv.size() - 1]);
			CalibrationSDGoodnessOfFit.MLL = AnsiString(sv[sv.size() - 1].c_str());

			GetNoCommentLine(ifile, s);		// "AIC:"
			sv = SplitString(s, ":");
			EraseSpaces(sv[sv.size() - 1]);
			CalibrationSDGoodnessOfFit.AIC = AnsiString(sv[sv.size() - 1].c_str());

			GetNoCommentLine(ifile, s);		// "Survival probability prediction error (SPPE) for each treatment:"
			GetNoCommentLine(ifile, s);		// "Data set	Treatment	Value"
			CalibrationSDGoodnessOfFit.SPPE.resize(0);
			getline(ifile, s);              // get first line
			while (s.substr(0, 1) != "#")
			{
				CalibrationSDGoodnessOfFit.SPPE.push_back(AnsiString(s.c_str()));
				getline(ifile, s);
			}

			// SD LCx
			LCxSD_out.resize(0);
			GetNoCommentLine(ifile, s);
			while (s.substr(0, 1) != "#")
			{
				LCxSD_out.push_back(AnsiString(s.c_str()));
				getline(ifile, s);
			}

			// IT results
			PMatPrintIT.BestValue.resize(gutsPARAMETER_NO);
			PMatPrintIT.LowerCI.resize(gutsPARAMETER_NO);
			PMatPrintIT.LowerCIWarning.resize(gutsPARAMETER_NO);
			PMatPrintIT.UpperCI.resize(gutsPARAMETER_NO);
			PMatPrintIT.UpperCIWarning.resize(gutsPARAMETER_NO);
			PMatPrintIT.LowerEdgeJoint95.resize(gutsPARAMETER_NO);
			PMatPrintIT.UpperEdgeJoint95.resize(gutsPARAMETER_NO);
			PMatPrintIT.Broken.resize(gutsPARAMETER_NO);
			GetNoCommentLine(ifile, s);		// "Parameter\tValue\tLower CI\tLower CI warning\tUpper CI\tUpper CI warning"
			for (int p = 0; p < gutsPARAMETER_NO; p++)
			{
				GetNoCommentLine(ifile, s);
				sv = SplitString(s, "\t");
				if (sv[1] != MISSING_DATA_SIGN)
					PMatPrintIT.BestValue[p] = AnsiString(sv[1].c_str()).ToDouble();
				else
					PMatPrintIT.BestValue[p] = -1;
				if (sv[2] != MISSING_DATA_SIGN)
					PMatPrintIT.LowerCI[p] = AnsiString(sv[2].c_str()).ToDouble();
				else
					PMatPrintIT.LowerCI[p] = -1;
				if (sv[3] != MISSING_DATA_SIGN)
					PMatPrintIT.LowerCIWarning[p] = sv[3];
				else
					PMatPrintIT.LowerCIWarning[p] = "-";
				if (sv[4] != MISSING_DATA_SIGN)
					PMatPrintIT.UpperCI[p] = AnsiString(sv[4].c_str()).ToDouble();
				else
					PMatPrintIT.UpperCI[p] = -1;
				if (sv[5] != MISSING_DATA_SIGN)
					PMatPrintIT.UpperCIWarning[p] = sv[5];
				else
					PMatPrintIT.UpperCIWarning[p] = "-";
			}

			// IT Goodness of fit
			GetNoCommentLine(ifile, s);		// "Model efficiency (NSE, r-square):"
			sv = SplitString(s, ":");
			EraseSpaces(sv[sv.size() - 1]);
			CalibrationITGoodnessOfFit.R_square = AnsiString(sv[sv.size() - 1].c_str());

			GetNoCommentLine(ifile, s);		// "Normalised root-means-square error (NRMSE):"
			sv = SplitString(s, ":");
			EraseSpaces(sv[sv.size() - 1]);
			CalibrationITGoodnessOfFit.NRMSE = AnsiString(sv[sv.size() - 1].c_str());

			GetNoCommentLine(ifile, s);		// "Minus log-likelihood (MLL):"
			sv = SplitString(s, ":");
			EraseSpaces(sv[sv.size() - 1]);
			CalibrationITGoodnessOfFit.MLL = AnsiString(sv[sv.size() - 1].c_str());

			GetNoCommentLine(ifile, s);		// "AIC:"
			sv = SplitString(s, ":");
			EraseSpaces(sv[sv.size() - 1]);
			CalibrationITGoodnessOfFit.AIC = AnsiString(sv[sv.size() - 1].c_str());

			GetNoCommentLine(ifile, s);		// "Survival probability prediction error (SPPE) for each treatment:"
			GetNoCommentLine(ifile, s);		// "Data set	Treatment	Value"
			CalibrationITGoodnessOfFit.SPPE.resize(0);
			getline(ifile, s);      		// get first line
			while (s.substr(0, 1) != "#")
			{
				CalibrationITGoodnessOfFit.SPPE.push_back(AnsiString(s.c_str()));
				getline(ifile, s);
			}

			// IT LCx
			LCxIT_out.resize(0);
			GetNoCommentLine(ifile, s);
			while (s.substr(0, 1) != "#")
			{
				LCxIT_out.push_back(AnsiString(s.c_str()));
				getline(ifile, s);
			}

			// adjust values of PMatSD and PMatIT if possible
			PMatSD.Value = PMatPrintSD.BestValue;
			PMatIT.Value = PMatPrintIT.BestValue;
		}
		else
		{
			PMatSD.Value = PMatSD.Max;
			PMatIT.Value = PMatIT.Max;
		}

		// display the parameters in the GUI
		bool change_value = true;
		bool change_range = true;
		bool remember_calibration_performed = CalibrationPerformed;
		DisplayParameterMatrixInGUI(PMatSD, gutsSD, change_value, change_range);
		DisplayParameterMatrixInGUI(PMatIT, gutsIT, change_value, change_range);
		CalibrationPerformed = remember_calibration_performed;

		// ################ VALIDATION DATA ###################
		GetNoCommentLine(ifile, s); // "SD validation performed: "
		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		if (sv[sv.size() - 1] == "Yes")
			SDValidationPerformed = true;
		else if (sv[sv.size() - 1] == "No")
			SDValidationPerformed = false;
		else
			return false;

		GetNoCommentLine(ifile, s); // "IT validation performed: "
		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		if (sv[sv.size() - 1] == "Yes")
			ITValidationPerformed = true;
		else if (sv[sv.size() - 1] == "No")
			ITValidationPerformed = false;
		else
			return false;

		GetNoCommentLine(ifile, s); // "Pre-fit background mortality to control: "
		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		if (sv[sv.size() - 1] == "Yes")
			FitHBToValidationControl = true;
		else if (sv[sv.size() - 1] == "No")
			FitHBToValidationControl = false;
		else
			return false;

		ValidationDataPresent = false;
		if (!WriteConcSurvInputFileToGUI(ifile, ValidationDataDescriptionRichEdit, ValidationSurvStringGrid,
				ValidationConcStringGrid, ConcUnitEdit, ValidControlCombo))
			return false;
		if (ValidationDataPresent && (SDValidationPerformed || ITValidationPerformed))
			if (!CheckAndGetValidationData(ValidationData))
				return false;
		if (SDValidationPerformed)
		{
			// SD Goodness of fit
			GetNoCommentLine(ifile, s);		// "Model efficiency (NSE, r-square):"
			sv = SplitString(s, ":");
			EraseSpaces(sv[sv.size() - 1]);
			ValidationSDGoodnessOfFit.R_square = AnsiString(sv[sv.size() - 1].c_str());

			GetNoCommentLine(ifile, s);		// "Normalised root-means-square error (NRMSE):"
			sv = SplitString(s, ":");
			EraseSpaces(sv[sv.size() - 1]);
			ValidationSDGoodnessOfFit.NRMSE = AnsiString(sv[sv.size() - 1].c_str());

			GetNoCommentLine(ifile, s);		// "Survival probability prediction error (SPPE) for each treatment:"
			GetNoCommentLine(ifile, s);		// "Data set	Treatment	Value"
			ValidationSDGoodnessOfFit.SPPE.resize(0);
			getline(ifile, s);      		// get first line
			while (s.substr(0, 1) != "#")
			{
				ValidationSDGoodnessOfFit.SPPE.push_back(AnsiString(s.c_str()));
				getline(ifile, s);
			}
		}
		if (ITValidationPerformed)
		{
			// SD Goodness of fit
			GetNoCommentLine(ifile, s);		// "Model efficiency (NSE, r-square):"
			sv = SplitString(s, ":");
			EraseSpaces(sv[sv.size() - 1]);
			ValidationITGoodnessOfFit.R_square = AnsiString(sv[sv.size() - 1].c_str());

			GetNoCommentLine(ifile, s);		// "Normalised root-means-square error (NRMSE):"
			sv = SplitString(s, ":");
			EraseSpaces(sv[sv.size() - 1]);
			ValidationITGoodnessOfFit.NRMSE = AnsiString(sv[sv.size() - 1].c_str());

			GetNoCommentLine(ifile, s);		// "Survival probability prediction error (SPPE) for each treatment:"
			GetNoCommentLine(ifile, s);		// "Data set	Treatment	Value"
			ValidationITGoodnessOfFit.SPPE.resize(0);
			getline(ifile, s);          	// get first line
			while (s.substr(0, 1) != "#")
			{
				ValidationITGoodnessOfFit.SPPE.push_back(AnsiString(s.c_str()));
				getline(ifile, s);
			}
		}

		// ################ PREDICTIONS DATA ###################
		PredictionsDataPresent = false;
		if (!WritePredictionsConcDataToGUI(ifile, PredictionsConcStringGrid))
			return false;
		if (PredictionsDataPresent)
		{
			GetDataFromGrid(PredictionsConcStringGrid, SingleExposureProfile);

			// build data structure
			PredictionsData.resize(1);
			PredictionsData[0].Concentration = SingleExposureProfile;

			// add dummy survival data as proposed by Tjalling Jager
			PredictionsData[0].Survival.Times.resize(2);
			PredictionsData[0].Survival.Times[0] = 0;
			PredictionsData[0].Survival.Times[1] = SingleExposureProfile.Times[SingleExposureProfile.Times.size() - 1];
		}

		// ############## PARAMETER SPACES ###################

		GetNoCommentLine(ifile, s); // "Number of parameter sets for SD:\t"
		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		NumberOfParameterSamplesSD = AnsiString(sv[sv.size() - 1].c_str()).ToInt();
		GetNoCommentLine(ifile, s);
		// "Parameter set\tkd\tmw\thb\tbw\tFs\tlog-likelihood"
		CalibrationParameterSpaceSD.resize(0);
		for (int ns = 0; ns < NumberOfParameterSamplesSD; ns++)
		{
			GetNoCommentLine(ifile, s);
			sv = SplitString(s, "\t");
			vector<double>param_temp;
			param_temp.resize(0);
			for (int p = 1; p < sv.size() - 1; p++)
				param_temp.push_back(AnsiString(sv[p].c_str()).ToDouble());
			CalibrationParameterSpaceSD.resize(CalibrationParameterSpaceSD.size() + 1);
			CalibrationParameterSpaceSD[CalibrationParameterSpaceSD.size() - 1].Parameter = param_temp;
			CalibrationParameterSpaceSD[CalibrationParameterSpaceSD.size() - 1].Likelihood =
					AnsiString(sv[sv.size() - 1].c_str()).ToDouble();
		}

		GetNoCommentLine(ifile, s); // "Number of parameter sets for IT:\t"
		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		NumberOfParameterSamplesIT = AnsiString(sv[sv.size() - 1].c_str()).ToInt();
		GetNoCommentLine(ifile, s);
		// "Parameter set\tkd\tmw\thb\tbw\tFs\tlog-likelihood"
		CalibrationParameterSpaceIT.resize(0);
		for (int ns = 0; ns < NumberOfParameterSamplesIT; ns++)
		{
			GetNoCommentLine(ifile, s);
			sv = SplitString(s, "\t");
			vector<double>param_temp;
			param_temp.resize(0);
			for (int p = 1; p < sv.size() - 1; p++)
				param_temp.push_back(AnsiString(sv[p].c_str()).ToDouble());
			CalibrationParameterSpaceIT.resize(CalibrationParameterSpaceIT.size() + 1);
			CalibrationParameterSpaceIT[CalibrationParameterSpaceIT.size() - 1].Parameter = param_temp;
			CalibrationParameterSpaceIT[CalibrationParameterSpaceIT.size() - 1].Likelihood =
					AnsiString(sv[sv.size() - 1].c_str()).ToDouble();
		}

		ifile.close();
		Screen->Cursor = crDefault;
	}
	catch (...) {
		Screen->Cursor = crDefault;
		return false;
	}

	return true;
}
// ---------------------------------------------------------------------------

bool TMainForm::OpenProjectFromFile_v1_2(ifstream &ifile)
{
	string s;
	vector<string> sv;

	// PROJECT DESCRIPTION
	GetNoCommentLine(ifile, s);
	ProjectDescription = s;
	getline(ifile, s);
	while (s.length() == 0 || (s.length() > 0 && s.substr(0, 1) != "#"))
		ProjectDescription += "\r\n" + s;
	ProjectDescriptionDlg->ProjectDescriptionRichEdit->Text = ProjectDescription.c_str();

	// #####################   CALIBRATION DATA   #####################
	GetNoCommentLine(ifile, s); // "Number of data sets:\t"
	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	int num_calib_sets = AnsiString(sv[sv.size() - 1].c_str()).ToInt();
	if (num_calib_sets <= MAX_CALIBRATION_DATA_SETS_NO)
		InputDataCalibSetNoSpin->Value = num_calib_sets;
	else
		return false;
	for (int d = 0; d < num_calib_sets; d++)
	{
		if (!WriteConcSurvInputFileToGUI(ifile, CalibDataDescriptionRichEdits[d], CalibSurvStringGrids[d],
				CalibConcStringGrids[d], ConcUnitEdit, CalibControlCombos[d]))
			return false;
	}
	if (!CheckAndGetCalibrationData(CalibrationData))
		return false;

	// #####################   CALIBRATION SETTINGS/RESULTS   #####################
	GetNoCommentLine(ifile, s); // "Calibration performed: "
	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	if (sv[sv.size() - 1] == "Yes")
		CalibrationPerformed = true;
	else if (sv[sv.size() - 1] == "No")
		CalibrationPerformed = false;
	else
		return false;

	GetNoCommentLine(ifile, s); // "Parameter ranges changed manually: "
	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	if (sv[sv.size() - 1] == "Yes")
		ParameterRangesManipulated = true;
	else if (sv[sv.size() - 1] == "No")
		ParameterRangesManipulated = false;
	else
		return false;

	GetNoCommentLine(ifile, s); // "Pre-fit background mortality to control: "
	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	if (sv[sv.size() - 1] == "Yes")
		PrefitHBToControl = true;
	else if (sv[sv.size() - 1] == "No")
		PrefitHBToControl = false;
	else
		return false;
	// now prepare the calibration data sets after the bg mortality info for e.g. another calibration
	Prepare_Data(CalibData_All, CalibData_HB, CalibrationData,
			NumberOfCalibrationDataSets, PrefitHBToControl);

	GetNoCommentLine(ifile, s); // "Expert settings: "
	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	if (sv[sv.size() - 1] == "Yes")
		ExpertSettingsOn = true;
	else if (sv[sv.size() - 1] == "No")
		ExpertSettingsOn = false;
	else
		return false;

	GetNoCommentLine(ifile, s); // "Use calculated parameter ranges (SD): "

	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	if (sv[sv.size() - 1] == "Yes")
		SDUseCalculatedParameterRangesForCalib = true;
	else if (sv[sv.size() - 1] == "No")
		SDUseCalculatedParameterRangesForCalib = false;
	else
		return false;

	GetNoCommentLine(ifile, s); // "Use existing parameter sample (SD): "

	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	if (sv[sv.size() - 1] == "Yes")
		SDUseExistingParameterSample = true;
	else if (sv[sv.size() - 1] == "No")
		SDUseExistingParameterSample = false;
	else
		return false;

	GetNoCommentLine(ifile, s); // "Use calculated parameter ranges (IT): "

	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	if (sv[sv.size() - 1] == "Yes")
		ITUseCalculatedParameterRangesForCalib = true;
	else if (sv[sv.size() - 1] == "No")
		ITUseCalculatedParameterRangesForCalib = false;
	else
		return false;

	GetNoCommentLine(ifile, s); // "Use existing parameter sample (IT): "

	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	if (sv[sv.size() - 1] == "Yes")
		ITUseExistingParameterSample = true;
	else if (sv[sv.size() - 1] == "No")
		ITUseExistingParameterSample = false;
	else
		return false;

	GetNoCommentLine(ifile, s); // "Parameter	fit y/n	Min	Max	norm/log"

	for (int p = 0; p < gutsPARAMETER_NO; p++)
	{
		GetNoCommentLine(ifile, s);
		sv = SplitString(s, "\t");
		if (sv[1] != MISSING_DATA_SIGN)
			PMatSD.Fit[p] = AnsiString(sv[1].c_str()).ToInt();
		else
			PMatSD.Fit[p] = -1;
		if (sv[2] != MISSING_DATA_SIGN)
			PMatSD.Min[p] = AnsiString(sv[2].c_str()).ToDouble();
		else
			PMatSD.Min[p] = -1;
		if (sv[3] != MISSING_DATA_SIGN)

			PMatSD.Max[p] = AnsiString(sv[3].c_str()).ToDouble();
		else
			PMatSD.Max[p] = -1;
		if (sv[4] != MISSING_DATA_SIGN)
			PMatSD.Scale[p] = AnsiString(sv[4].c_str()).ToInt();
		else
			PMatSD.Scale[p] = -1;
	}
	GetNoCommentLine(ifile, s); // "Parameter	fit y/n	Min	Max	norm/log"
	for (int p = 0; p < gutsPARAMETER_NO; p++)
	{
		GetNoCommentLine(ifile, s);
		sv = SplitString(s, "\t");
		if (sv[1] != MISSING_DATA_SIGN)
			PMatIT.Fit[p] = AnsiString(sv[1].c_str()).ToInt();
		else
			PMatIT.Fit[p] = -1;
		if (sv[2] != MISSING_DATA_SIGN)
			PMatIT.Min[p] = AnsiString(sv[2].c_str()).ToDouble();
		else
			PMatIT.Min[p] = -1;
		if (sv[3] != MISSING_DATA_SIGN)
			PMatIT.Max[p] = AnsiString(sv[3].c_str()).ToDouble();
		else
			PMatIT.Max[p] = -1;
		if (sv[4] != MISSING_DATA_SIGN)
			PMatIT.Scale[p] = AnsiString(sv[4].c_str()).ToInt();
		else
			PMatIT.Scale[p] = -1;
	}
	if (CalibrationPerformed)
	{
		// SD results
		PMatPrintSD.BestValue.resize(gutsPARAMETER_NO);
		PMatPrintSD.LowerCI.resize(gutsPARAMETER_NO);
		PMatPrintSD.LowerCIWarning.resize(gutsPARAMETER_NO);
		PMatPrintSD.UpperCI.resize(gutsPARAMETER_NO);
		PMatPrintSD.UpperCIWarning.resize(gutsPARAMETER_NO);
		PMatPrintSD.LowerEdgeJoint95.resize(gutsPARAMETER_NO);
		PMatPrintSD.UpperEdgeJoint95.resize(gutsPARAMETER_NO);
		PMatPrintSD.Broken.resize(gutsPARAMETER_NO);
		GetNoCommentLine(ifile, s);		// "Parameter\tValue\tLower CI\tLower CI warning\tUpper CI\tUpper CI warning"
		for (int p = 0; p < gutsPARAMETER_NO; p++)
		{
			GetNoCommentLine(ifile, s);
			sv = SplitString(s, "\t");
			if (sv[1] != MISSING_DATA_SIGN)
				PMatPrintSD.BestValue[p] = AnsiString(sv[1].c_str()).ToDouble();
			else
				PMatPrintSD.BestValue[p] = -1;
			if (sv[2] != MISSING_DATA_SIGN)
				PMatPrintSD.LowerCI[p] = AnsiString(sv[2].c_str()).ToDouble();
			else
				PMatPrintSD.LowerCI[p] = -1;
			if (sv[3] != MISSING_DATA_SIGN)
				PMatPrintSD.LowerCIWarning[p] = sv[3];
			else
				PMatPrintSD.LowerCIWarning[p] = "-";
			if (sv[4] != MISSING_DATA_SIGN)
				PMatPrintSD.UpperCI[p] = AnsiString(sv[4].c_str()).ToDouble();
			else
				PMatPrintSD.UpperCI[p] = -1;
			if (sv[5] != MISSING_DATA_SIGN)
				PMatPrintSD.UpperCIWarning[p] = sv[5];
			else
				PMatPrintSD.UpperCIWarning[p] = "-";
		}

		// SD Goodness of fit
		GetNoCommentLine(ifile, s);		// "Model efficiency (NSE, r-square):"
		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		CalibrationSDGoodnessOfFit.R_square = AnsiString(sv[sv.size() - 1].c_str());

		GetNoCommentLine(ifile, s);		// "Normalised root-means-square error (NRMSE):"
		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		CalibrationSDGoodnessOfFit.NRMSE = AnsiString(sv[sv.size() - 1].c_str());

		GetNoCommentLine(ifile, s);		// "Survival probability prediction error (SPPE) for each treatment:"
		GetNoCommentLine(ifile, s);		// "Data set	Treatment	Value"
		CalibrationSDGoodnessOfFit.SPPE.resize(0);
		getline(ifile, s);              // get first line
		while (s.substr(0, 1) != "#")
		{
			CalibrationSDGoodnessOfFit.SPPE.push_back(AnsiString(s.c_str()));
			getline(ifile, s);
		}

		// SD LCx
		LCxSD_out.resize(0);
		GetNoCommentLine(ifile, s);
		while (s.substr(0, 1) != "#")
		{
			LCxSD_out.push_back(AnsiString(s.c_str()));
			getline(ifile, s);
		}

		// IT results
		PMatPrintIT.BestValue.resize(gutsPARAMETER_NO);
		PMatPrintIT.LowerCI.resize(gutsPARAMETER_NO);
		PMatPrintIT.LowerCIWarning.resize(gutsPARAMETER_NO);
		PMatPrintIT.UpperCI.resize(gutsPARAMETER_NO);
		PMatPrintIT.UpperCIWarning.resize(gutsPARAMETER_NO);
		PMatPrintIT.LowerEdgeJoint95.resize(gutsPARAMETER_NO);
		PMatPrintIT.UpperEdgeJoint95.resize(gutsPARAMETER_NO);
		PMatPrintIT.Broken.resize(gutsPARAMETER_NO);
		GetNoCommentLine(ifile, s);		// "Parameter\tValue\tLower CI\tLower CI warning\tUpper CI\tUpper CI warning"
		for (int p = 0; p < gutsPARAMETER_NO; p++)
		{
			GetNoCommentLine(ifile, s);
			sv = SplitString(s, "\t");
			if (sv[1] != MISSING_DATA_SIGN)
				PMatPrintIT.BestValue[p] = AnsiString(sv[1].c_str()).ToDouble();
			else
				PMatPrintIT.BestValue[p] = -1;
			if (sv[2] != MISSING_DATA_SIGN)
				PMatPrintIT.LowerCI[p] = AnsiString(sv[2].c_str()).ToDouble();
			else
				PMatPrintIT.LowerCI[p] = -1;
			if (sv[3] != MISSING_DATA_SIGN)
				PMatPrintIT.LowerCIWarning[p] = sv[3];
			else
				PMatPrintIT.LowerCIWarning[p] = "-";
			if (sv[4] != MISSING_DATA_SIGN)
				PMatPrintIT.UpperCI[p] = AnsiString(sv[4].c_str()).ToDouble();
			else
				PMatPrintIT.UpperCI[p] = -1;
			if (sv[5] != MISSING_DATA_SIGN)
				PMatPrintIT.UpperCIWarning[p] = sv[5];
			else
				PMatPrintIT.UpperCIWarning[p] = "-";
		}

		// IT Goodness of fit
		GetNoCommentLine(ifile, s);		// "Model efficiency (NSE, r-square):"
		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		CalibrationITGoodnessOfFit.R_square = AnsiString(sv[sv.size() - 1].c_str());

		GetNoCommentLine(ifile, s);		// "Normalised root-means-square error (NRMSE):"
		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		CalibrationITGoodnessOfFit.NRMSE = AnsiString(sv[sv.size() - 1].c_str());

		GetNoCommentLine(ifile, s);		// "Survival probability prediction error (SPPE) for each treatment:"
		GetNoCommentLine(ifile, s);		// "Data set	Treatment	Value"
		CalibrationITGoodnessOfFit.SPPE.resize(0);
		getline(ifile, s);      		// get first line
		while (s.substr(0, 1) != "#")
		{
			CalibrationITGoodnessOfFit.SPPE.push_back(AnsiString(s.c_str()));
			getline(ifile, s);
		}

		// IT LCx
		LCxIT_out.resize(0);
		GetNoCommentLine(ifile, s);
		while (s.substr(0, 1) != "#")
		{
			LCxIT_out.push_back(AnsiString(s.c_str()));
			getline(ifile, s);
		}

		// adjust values of PMatSD and PMatIT if possible
		PMatSD.Value = PMatPrintSD.BestValue;
		PMatIT.Value = PMatPrintIT.BestValue;
	}
	else
	{
		PMatSD.Value = PMatSD.Max;
		PMatIT.Value = PMatIT.Max;
	}

	// display the parameters in the GUI
	bool change_value = true;
	bool change_range = true;
	bool remember_calibration_performed = CalibrationPerformed;
	DisplayParameterMatrixInGUI(PMatSD, gutsSD, change_value, change_range);
	DisplayParameterMatrixInGUI(PMatIT, gutsIT, change_value, change_range);
	CalibrationPerformed = remember_calibration_performed;

	// ################ VALIDATION DATA ###################
	GetNoCommentLine(ifile, s); // "SD validation performed: "
	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	if (sv[sv.size() - 1] == "Yes")
		SDValidationPerformed = true;
	else if (sv[sv.size() - 1] == "No")
		SDValidationPerformed = false;
	else
		return false;

	GetNoCommentLine(ifile, s); // "IT validation performed: "
	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	if (sv[sv.size() - 1] == "Yes")
		ITValidationPerformed = true;
	else if (sv[sv.size() - 1] == "No")
		ITValidationPerformed = false;
	else
		return false;

	GetNoCommentLine(ifile, s); // "Pre-fit background mortality to control: "
	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	if (sv[sv.size() - 1] == "Yes")
		FitHBToValidationControl = true;
	else if (sv[sv.size() - 1] == "No")
		FitHBToValidationControl = false;
	else
		return false;

	ValidationDataPresent = false;
	if (!WriteConcSurvInputFileToGUI(ifile, ValidationDataDescriptionRichEdit, ValidationSurvStringGrid,
			ValidationConcStringGrid, ConcUnitEdit, ValidControlCombo))
		return false;
	if (ValidationDataPresent && (SDValidationPerformed || ITValidationPerformed))
		if (!CheckAndGetValidationData(ValidationData))
			return false;
	if (SDValidationPerformed)
	{
		// SD Goodness of fit
		GetNoCommentLine(ifile, s);		// "Model efficiency (NSE, r-square):"
		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		ValidationSDGoodnessOfFit.R_square = AnsiString(sv[sv.size() - 1].c_str());

		GetNoCommentLine(ifile, s);		// "Normalised root-means-square error (NRMSE):"
		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		ValidationSDGoodnessOfFit.NRMSE = AnsiString(sv[sv.size() - 1].c_str());

		GetNoCommentLine(ifile, s);		// "Survival probability prediction error (SPPE) for each treatment:"
		GetNoCommentLine(ifile, s);		// "Data set	Treatment	Value"
		ValidationSDGoodnessOfFit.SPPE.resize(0);
		getline(ifile, s);      		// get first line
		while (s.substr(0, 1) != "#")
		{
			ValidationSDGoodnessOfFit.SPPE.push_back(AnsiString(s.c_str()));
            getline(ifile, s);
		}
	}
	if (ITValidationPerformed)
	{
        // SD Goodness of fit
		GetNoCommentLine(ifile, s);		// "Model efficiency (NSE, r-square):"
		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		ValidationITGoodnessOfFit.R_square = AnsiString(sv[sv.size() - 1].c_str());

		GetNoCommentLine(ifile, s);		// "Normalised root-means-square error (NRMSE):"
		sv = SplitString(s, ":");
		EraseSpaces(sv[sv.size() - 1]);
		ValidationITGoodnessOfFit.NRMSE = AnsiString(sv[sv.size() - 1].c_str());

		GetNoCommentLine(ifile, s);		// "Survival probability prediction error (SPPE) for each treatment:"
		GetNoCommentLine(ifile, s);		// "Data set	Treatment	Value"
		ValidationITGoodnessOfFit.SPPE.resize(0);
		getline(ifile, s);          	// get first line
		while (s.substr(0, 1) != "#")
		{
			ValidationITGoodnessOfFit.SPPE.push_back(AnsiString(s.c_str()));
			getline(ifile, s);
		}
	}

	// ################ PREDICTIONS DATA ###################
	PredictionsDataPresent = false;
	if (!WritePredictionsConcDataToGUI(ifile, PredictionsConcStringGrid))
		return false;
	if (PredictionsDataPresent)
	{
		GetDataFromGrid(PredictionsConcStringGrid, SingleExposureProfile);

		// build data structure
		PredictionsData.resize(1);
		PredictionsData[0].Concentration = SingleExposureProfile;

		// add dummy survival data as proposed by Tjalling Jager
		PredictionsData[0].Survival.Times.resize(2);
		PredictionsData[0].Survival.Times[0] = 0;
		PredictionsData[0].Survival.Times[1] = SingleExposureProfile.Times[SingleExposureProfile.Times.size() - 1];
	}

	// ############## PARAMETER SPACES ###################

	GetNoCommentLine(ifile, s); // "Number of parameter sets for SD:\t"
	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	NumberOfParameterSamplesSD = AnsiString(sv[sv.size() - 1].c_str()).ToInt();
	GetNoCommentLine(ifile, s);
	// "Parameter set\tkd\tmw\thb\tbw\tFs\tlog-likelihood"
	CalibrationParameterSpaceSD.resize(0);
	for (int ns = 0; ns < NumberOfParameterSamplesSD; ns++)
	{
		GetNoCommentLine(ifile, s);
		sv = SplitString(s, "\t");
		vector<double>param_temp;
		param_temp.resize(0);
		for (int p = 1; p < sv.size() - 1; p++)
			param_temp.push_back(AnsiString(sv[p].c_str()).ToDouble());
		CalibrationParameterSpaceSD.resize(CalibrationParameterSpaceSD.size() + 1);
		CalibrationParameterSpaceSD[CalibrationParameterSpaceSD.size() - 1].Parameter = param_temp;
		CalibrationParameterSpaceSD[CalibrationParameterSpaceSD.size() - 1].Likelihood =
				AnsiString(sv[sv.size() - 1].c_str()).ToDouble();
	}

	GetNoCommentLine(ifile, s); // "Number of parameter sets for IT:\t"
	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	NumberOfParameterSamplesIT = AnsiString(sv[sv.size() - 1].c_str()).ToInt();
	GetNoCommentLine(ifile, s);
	// "Parameter set\tkd\tmw\thb\tbw\tFs\tlog-likelihood"
	CalibrationParameterSpaceIT.resize(0);
	for (int ns = 0; ns < NumberOfParameterSamplesIT; ns++)
	{
		GetNoCommentLine(ifile, s);
		sv = SplitString(s, "\t");
		vector<double>param_temp;
		param_temp.resize(0);
		for (int p = 1; p < sv.size() - 1; p++)
			param_temp.push_back(AnsiString(sv[p].c_str()).ToDouble());
		CalibrationParameterSpaceIT.resize(CalibrationParameterSpaceIT.size() + 1);
		CalibrationParameterSpaceIT[CalibrationParameterSpaceIT.size() - 1].Parameter = param_temp;
		CalibrationParameterSpaceIT[CalibrationParameterSpaceIT.size() - 1].Likelihood =
				AnsiString(sv[sv.size() - 1].c_str()).ToDouble();
	}

	ifile.close();
	Screen->Cursor = crArrow;

	return true;
}
// ---------------------------------------------------------------------------

bool TMainForm::OpenProjectFromFile_v1_1(ifstream &ifile)
{
	string s;
	vector<string>sv;

	// PROJECT DESCRIPTION
	GetNoCommentLine(ifile, s);
	ProjectDescription = s;
	getline(ifile, s);
	while (s.length() == 0 || (s.length() > 0 && s.substr(0, 1) != "#"))
		ProjectDescription += "\r\n" + s;
	ProjectDescriptionDlg->ProjectDescriptionRichEdit->Text = ProjectDescription.c_str();

	// #####################   CALIBRATION DATA   #####################
	GetNoCommentLine(ifile, s); // "Number of data sets:\t"
	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	int num_calib_sets = AnsiString(sv[sv.size() - 1].c_str()).ToInt();
	if (num_calib_sets <= MAX_CALIBRATION_DATA_SETS_NO)
		InputDataCalibSetNoSpin->Value = num_calib_sets;
	else
		return false;
	for (int d = 0; d < num_calib_sets; d++)
	{
		if (!WriteConcSurvInputFileToGUI(ifile, CalibDataDescriptionRichEdits[d], CalibSurvStringGrids[d],
				CalibConcStringGrids[d], ConcUnitEdit, CalibControlCombos[d]))
			return false;
	}
	if (!CheckAndGetCalibrationData(CalibrationData))
		return false;

	// #####################   CALIBRATION SETTINGS/RESULTS   #####################
	GetNoCommentLine(ifile, s); // "Calibration performed: "
	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	if (sv[sv.size() - 1] == "Yes")
		CalibrationPerformed = true;
	else if (sv[sv.size() - 1] == "No")
		CalibrationPerformed = false;
	else
		return false;

	GetNoCommentLine(ifile, s); // "Pre-fit background mortality to control: "
	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	if (sv[sv.size() - 1] == "Yes")
		PrefitHBToControl = true;
	else if (sv[sv.size() - 1] == "No")
		PrefitHBToControl = false;
	else
		return false;

	GetNoCommentLine(ifile, s); // "Expert settings: "
	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	if (sv[sv.size() - 1] == "Yes")
		ExpertSettingsOn = true;
	else if (sv[sv.size() - 1] == "No")
		ExpertSettingsOn = false;
	else
		return false;

	GetNoCommentLine(ifile, s); // "Use calculated parameter ranges (SD): "

	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	if (sv[sv.size() - 1] == "Yes")
		SDUseCalculatedParameterRangesForCalib = true;
	else if (sv[sv.size() - 1] == "No")
		SDUseCalculatedParameterRangesForCalib = false;
	else
		return false;

	GetNoCommentLine(ifile, s); // "Use existing parameter sample (SD): "

	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	if (sv[sv.size() - 1] == "Yes")
		SDUseExistingParameterSample = true;
	else if (sv[sv.size() - 1] == "No")
		SDUseExistingParameterSample = false;
	else
		return false;

	GetNoCommentLine(ifile, s); // "Use calculated parameter ranges (IT): "

	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	if (sv[sv.size() - 1] == "Yes")
		ITUseCalculatedParameterRangesForCalib = true;
	else if (sv[sv.size() - 1] == "No")
		ITUseCalculatedParameterRangesForCalib = false;
	else
		return false;

	GetNoCommentLine(ifile, s); // "Use existing parameter sample (IT): "

	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	if (sv[sv.size() - 1] == "Yes")
		ITUseExistingParameterSample = true;
	else if (sv[sv.size() - 1] == "No")
		ITUseExistingParameterSample = false;
	else
		return false;

	GetNoCommentLine(ifile, s); // "Parameter	fit y/n	Min	Max	norm/log"

	for (int p = 0; p < gutsPARAMETER_NO; p++)
	{
		GetNoCommentLine(ifile, s);
		sv = SplitString(s, "\t");
		if (sv[1] != MISSING_DATA_SIGN)
			PMatSD.Fit[p] = AnsiString(sv[1].c_str()).ToInt();
		else
			PMatSD.Fit[p] = -1;
		if (sv[2] != MISSING_DATA_SIGN)
			PMatSD.Min[p] = AnsiString(sv[2].c_str()).ToDouble();
		else
			PMatSD.Min[p] = -1;
		if (sv[3] != MISSING_DATA_SIGN)

			PMatSD.Max[p] = AnsiString(sv[3].c_str()).ToDouble();
		else
			PMatSD.Max[p] = -1;
		if (sv[4] != MISSING_DATA_SIGN)
			PMatSD.Scale[p] = AnsiString(sv[4].c_str()).ToInt();
		else
			PMatSD.Scale[p] = -1;
	}
	GetNoCommentLine(ifile, s); // "Parameter	fit y/n	Min	Max	norm/log"
	for (int p = 0; p < gutsPARAMETER_NO; p++)
	{
		GetNoCommentLine(ifile, s);
		sv = SplitString(s, "\t");
		if (sv[1] != MISSING_DATA_SIGN)
			PMatIT.Fit[p] = AnsiString(sv[1].c_str()).ToInt();
		else
			PMatIT.Fit[p] = -1;
		if (sv[2] != MISSING_DATA_SIGN)
			PMatIT.Min[p] = AnsiString(sv[2].c_str()).ToDouble();
		else
			PMatIT.Min[p] = -1;
		if (sv[3] != MISSING_DATA_SIGN)
			PMatIT.Max[p] = AnsiString(sv[3].c_str()).ToDouble();
		else
			PMatIT.Max[p] = -1;
		if (sv[4] != MISSING_DATA_SIGN)
			PMatIT.Scale[p] = AnsiString(sv[4].c_str()).ToInt();
		else
			PMatIT.Scale[p] = -1;
	}
	if (CalibrationPerformed)
	{
		PMatPrintSD.BestValue.resize(gutsPARAMETER_NO);
		PMatPrintSD.LowerCI.resize(gutsPARAMETER_NO);
		PMatPrintSD.LowerCIWarning.resize(gutsPARAMETER_NO);
		PMatPrintSD.UpperCI.resize(gutsPARAMETER_NO);
		PMatPrintSD.UpperCIWarning.resize(gutsPARAMETER_NO);
		PMatPrintSD.LowerEdgeJoint95.resize(gutsPARAMETER_NO);
		PMatPrintSD.UpperEdgeJoint95.resize(gutsPARAMETER_NO);
		PMatPrintSD.Broken.resize(gutsPARAMETER_NO);
		GetNoCommentLine(ifile, s);
		// "Parameter\tValue\tLower CI\tLower CI warning\tUpper CI\tUpper CI warning"
		for (int p = 0; p < gutsPARAMETER_NO; p++)
		{
			GetNoCommentLine(ifile, s);
			sv = SplitString(s, "\t");
			if (sv[1] != MISSING_DATA_SIGN)
				PMatPrintSD.BestValue[p] = AnsiString(sv[1].c_str()).ToDouble();
			else
				PMatPrintSD.BestValue[p] = -1;
			if (sv[2] != MISSING_DATA_SIGN)
				PMatPrintSD.LowerCI[p] = AnsiString(sv[2].c_str()).ToDouble();
			else
				PMatPrintSD.LowerCI[p] = -1;
			if (sv[3] != MISSING_DATA_SIGN)
				PMatPrintSD.LowerCIWarning[p] = sv[3];
			else
				PMatPrintSD.LowerCIWarning[p] = "-";
			if (sv[4] != MISSING_DATA_SIGN)
				PMatPrintSD.UpperCI[p] = AnsiString(sv[4].c_str()).ToDouble();
			else
				PMatPrintSD.UpperCI[p] = -1;
			if (sv[5] != MISSING_DATA_SIGN)
				PMatPrintSD.UpperCIWarning[p] = sv[5];
			else
				PMatPrintSD.UpperCIWarning[p] = "-";
		}
		PMatPrintIT.BestValue.resize(gutsPARAMETER_NO);
		PMatPrintIT.LowerCI.resize(gutsPARAMETER_NO);
		PMatPrintIT.LowerCIWarning.resize(gutsPARAMETER_NO);
		PMatPrintIT.UpperCI.resize(gutsPARAMETER_NO);
		PMatPrintIT.UpperCIWarning.resize(gutsPARAMETER_NO);
		PMatPrintIT.LowerEdgeJoint95.resize(gutsPARAMETER_NO);
		PMatPrintIT.UpperEdgeJoint95.resize(gutsPARAMETER_NO);
		PMatPrintIT.Broken.resize(gutsPARAMETER_NO);
		GetNoCommentLine(ifile, s);
		// "Parameter\tValue\tLower CI\tLower CI warning\tUpper CI\tUpper CI warning"
		for (int p = 0; p < gutsPARAMETER_NO; p++)
		{
			GetNoCommentLine(ifile, s);
			sv = SplitString(s, "\t");
			if (sv[1] != MISSING_DATA_SIGN)
				PMatPrintIT.BestValue[p] = AnsiString(sv[1].c_str()).ToDouble();
			else
				PMatPrintIT.BestValue[p] = -1;
			if (sv[2] != MISSING_DATA_SIGN)
				PMatPrintIT.LowerCI[p] = AnsiString(sv[2].c_str()).ToDouble();
			else
				PMatPrintIT.LowerCI[p] = -1;
			if (sv[3] != MISSING_DATA_SIGN)
				PMatPrintIT.LowerCIWarning[p] = sv[3];
			else
				PMatPrintIT.LowerCIWarning[p] = "-";
			if (sv[4] != MISSING_DATA_SIGN)
				PMatPrintIT.UpperCI[p] = AnsiString(sv[4].c_str()).ToDouble();
			else
				PMatPrintIT.UpperCI[p] = -1;
			if (sv[5] != MISSING_DATA_SIGN)
				PMatPrintIT.UpperCIWarning[p] = sv[5];
			else
				PMatPrintIT.UpperCIWarning[p] = "-";
		}
		// adjust values of PMatSD and PMatIT if possible
		PMatSD.Value = PMatPrintSD.BestValue;
		PMatIT.Value = PMatPrintIT.BestValue;
	}
	else
	{
		PMatSD.Value = PMatSD.Max;
		PMatIT.Value = PMatIT.Max;
	}

	// display the parameters in the GUI
	bool change_value = true;
	bool change_range = true;
	DisplayParameterMatrixInGUI(PMatSD, gutsSD, change_value, change_range);
	DisplayParameterMatrixInGUI(PMatIT, gutsIT, change_value, change_range);

	// ################ VALIDATION DATA ###################
	ValidationDataPresent = false;
	if (!WriteConcSurvInputFileToGUI(ifile, ValidationDataDescriptionRichEdit, ValidationSurvStringGrid,
			ValidationConcStringGrid, ConcUnitEdit, ValidControlCombo))
		return false;
	if (ValidationDataPresent)
		if (!CheckAndGetValidationData(ValidationData))
			return false;

	// ################ PREDICTIONS DATA ###################
	PredictionsDataPresent = false;
	if (!WritePredictionsConcDataToGUI(ifile, PredictionsConcStringGrid))
		return false;
	if (PredictionsDataPresent)
	{
		GetDataFromGrid(PredictionsConcStringGrid, SingleExposureProfile);

		// build data structure
		PredictionsData.resize(1);
		PredictionsData[0].Concentration = SingleExposureProfile;

		// add dummy survival data as proposed by Tjalling Jager
		PredictionsData[0].Survival.Times.resize(2);
		PredictionsData[0].Survival.Times[0] = 0;
		PredictionsData[0].Survival.Times[1] = SingleExposureProfile.Times[SingleExposureProfile.Times.size() - 1];
	}

	// ############## PARAMETER SPACES ###################

	GetNoCommentLine(ifile, s); // "Number of parameter sets for SD:\t"
	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	NumberOfParameterSamplesSD = AnsiString(sv[sv.size() - 1].c_str()).ToInt();
	GetNoCommentLine(ifile, s);
	// "Parameter set\tkd\tmw\thb\tbw\tFs\tlog-likelihood"
	CalibrationParameterSpaceSD.resize(0);
	for (int ns = 0; ns < NumberOfParameterSamplesSD; ns++)
	{
		GetNoCommentLine(ifile, s);
		sv = SplitString(s, "\t");
		vector<double>param_temp;
		param_temp.resize(0);
		for (int p = 1; p < sv.size() - 1; p++)
			param_temp.push_back(AnsiString(sv[p].c_str()).ToDouble());
		CalibrationParameterSpaceSD.resize(CalibrationParameterSpaceSD.size() + 1);
		CalibrationParameterSpaceSD[CalibrationParameterSpaceSD.size() - 1].Parameter = param_temp;
		CalibrationParameterSpaceSD[CalibrationParameterSpaceSD.size() - 1].Likelihood =
				AnsiString(sv[sv.size() - 1].c_str()).ToDouble();
	}

	GetNoCommentLine(ifile, s); // "Number of parameter sets for IT:\t"
	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	NumberOfParameterSamplesIT = AnsiString(sv[sv.size() - 1].c_str()).ToInt();
	GetNoCommentLine(ifile, s);
	// "Parameter set\tkd\tmw\thb\tbw\tFs\tlog-likelihood"
	CalibrationParameterSpaceIT.resize(0);
	for (int ns = 0; ns < NumberOfParameterSamplesIT; ns++)
	{
		GetNoCommentLine(ifile, s);
		sv = SplitString(s, "\t");
		vector<double>param_temp;
		param_temp.resize(0);
		for (int p = 1; p < sv.size() - 1; p++)
			param_temp.push_back(AnsiString(sv[p].c_str()).ToDouble());
		CalibrationParameterSpaceIT.resize(CalibrationParameterSpaceIT.size() + 1);
		CalibrationParameterSpaceIT[CalibrationParameterSpaceIT.size() - 1].Parameter = param_temp;
		CalibrationParameterSpaceIT[CalibrationParameterSpaceIT.size() - 1].Likelihood =
				AnsiString(sv[sv.size() - 1].c_str()).ToDouble();
	}

	ifile.close();
	Screen->Cursor = crArrow;

	return true;
}
// ---------------------------------------------------------------------------

bool TMainForm::OpenProjectFromFile_v1_0(ifstream &ifile)
{
	string s;
	vector<string>sv;

	// #####################   CALIBRATION DATA   #####################
	GetNoCommentLine(ifile, s); // "Number of data sets:\t"
	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	int num_calib_sets = AnsiString(sv[sv.size() - 1].c_str()).ToInt();
	if (num_calib_sets <= MAX_CALIBRATION_DATA_SETS_NO)
		InputDataCalibSetNoSpin->Value = num_calib_sets;
	else
		return false;
	for (int d = 0; d < num_calib_sets; d++)
	{
		if (!WriteConcSurvInputFileToGUI(ifile, CalibDataDescriptionRichEdits[d], CalibSurvStringGrids[d],
				CalibConcStringGrids[d], ConcUnitEdit, CalibControlCombos[d]))
			return false;
	}

	// #####################   CALIBRATION SETTINGS/RESULTS   #####################
	GetNoCommentLine(ifile, s); // "Calibration performed: "
	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	if (sv[sv.size() - 1] == "Yes")
		CalibrationPerformed = true;
	else if (sv[sv.size() - 1] == "No")
		CalibrationPerformed = false;
	else
		return false;

	GetNoCommentLine(ifile, s); // "Pre-fit background mortality to control: "
	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	if (sv[sv.size() - 1] == "Yes")
		PrefitHBToControl = true;
	else if (sv[sv.size() - 1] == "No")
		PrefitHBToControl = false;
	else
		return false;

	GetNoCommentLine(ifile, s); // "Expert settings: "
	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	if (sv[sv.size() - 1] == "Yes")
		ExpertSettingsOn = true;
	else if (sv[sv.size() - 1] == "No")
		ExpertSettingsOn = false;
	else
		return false;

	GetNoCommentLine(ifile, s); // "Use calculated parameter ranges (SD): "

	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	if (sv[sv.size() - 1] == "Yes")
		SDUseCalculatedParameterRangesForCalib = true;
	else if (sv[sv.size() - 1] == "No")
		SDUseCalculatedParameterRangesForCalib = false;
	else
		return false;

	GetNoCommentLine(ifile, s); // "Use existing parameter sample (SD): "

	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	if (sv[sv.size() - 1] == "Yes")
		SDUseExistingParameterSample = true;
	else if (sv[sv.size() - 1] == "No")
		SDUseExistingParameterSample = false;
	else
		return false;

	GetNoCommentLine(ifile, s); // "Use calculated parameter ranges (IT): "

	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	if (sv[sv.size() - 1] == "Yes")
		ITUseCalculatedParameterRangesForCalib = true;
	else if (sv[sv.size() - 1] == "No")
		ITUseCalculatedParameterRangesForCalib = false;
	else
		return false;

	GetNoCommentLine(ifile, s); // "Use existing parameter sample (IT): "

	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	if (sv[sv.size() - 1] == "Yes")
		ITUseExistingParameterSample = true;
	else if (sv[sv.size() - 1] == "No")
		ITUseExistingParameterSample = false;
	else
		return false;

	GetNoCommentLine(ifile, s); // "Parameter	fit y/n	Min	Max	norm/log"

	for (int p = 0; p < gutsPARAMETER_NO; p++)
	{
		GetNoCommentLine(ifile, s);
		sv = SplitString(s, "\t");
		if (sv[1] != MISSING_DATA_SIGN)
			PMatSD.Fit[p] = AnsiString(sv[1].c_str()).ToInt();
		else
			PMatSD.Fit[p] = -1;
		if (sv[2] != MISSING_DATA_SIGN)
			PMatSD.Min[p] = AnsiString(sv[2].c_str()).ToDouble();
		else
			PMatSD.Min[p] = -1;
		if (sv[3] != MISSING_DATA_SIGN)

			PMatSD.Max[p] = AnsiString(sv[3].c_str()).ToDouble();
		else
			PMatSD.Max[p] = -1;
		if (sv[4] != MISSING_DATA_SIGN)
			PMatSD.Scale[p] = AnsiString(sv[4].c_str()).ToInt();
		else
			PMatSD.Scale[p] = -1;
	}
	GetNoCommentLine(ifile, s); // "Parameter	fit y/n	Min	Max	norm/log"
	for (int p = 0; p < gutsPARAMETER_NO; p++)
	{
		GetNoCommentLine(ifile, s);
		sv = SplitString(s, "\t");
		if (sv[1] != MISSING_DATA_SIGN)
			PMatIT.Fit[p] = AnsiString(sv[1].c_str()).ToInt();
		else
			PMatIT.Fit[p] = -1;
		if (sv[2] != MISSING_DATA_SIGN)
			PMatIT.Min[p] = AnsiString(sv[2].c_str()).ToDouble();
		else
			PMatIT.Min[p] = -1;
		if (sv[3] != MISSING_DATA_SIGN)
			PMatIT.Max[p] = AnsiString(sv[3].c_str()).ToDouble();
		else
			PMatIT.Max[p] = -1;
		if (sv[4] != MISSING_DATA_SIGN)
			PMatIT.Scale[p] = AnsiString(sv[4].c_str()).ToInt();
		else
			PMatIT.Scale[p] = -1;
	}
	if (CalibrationPerformed)
	{
		PMatPrintSD.BestValue.resize(gutsPARAMETER_NO);
		PMatPrintSD.LowerCI.resize(gutsPARAMETER_NO);
		PMatPrintSD.LowerCIWarning.resize(gutsPARAMETER_NO);
		PMatPrintSD.UpperCI.resize(gutsPARAMETER_NO);
		PMatPrintSD.UpperCIWarning.resize(gutsPARAMETER_NO);
		PMatPrintSD.LowerEdgeJoint95.resize(gutsPARAMETER_NO);
		PMatPrintSD.UpperEdgeJoint95.resize(gutsPARAMETER_NO);
		PMatPrintSD.Broken.resize(gutsPARAMETER_NO);
		GetNoCommentLine(ifile, s);
		// "Parameter\tValue\tLower CI\tLower CI warning\tUpper CI\tUpper CI warning"
		for (int p = 0; p < gutsPARAMETER_NO; p++)
		{
			GetNoCommentLine(ifile, s);
			sv = SplitString(s, "\t");
			if (sv[1] != MISSING_DATA_SIGN)
				PMatPrintSD.BestValue[p] = AnsiString(sv[1].c_str()).ToDouble();
			else
				PMatPrintSD.BestValue[p] = -1;
			if (sv[2] != MISSING_DATA_SIGN)
				PMatPrintSD.LowerCI[p] = AnsiString(sv[2].c_str()).ToDouble();
			else
				PMatPrintSD.LowerCI[p] = -1;
			if (sv[3] != MISSING_DATA_SIGN)
				PMatPrintSD.LowerCIWarning[p] = sv[3];
			else
				PMatPrintSD.LowerCIWarning[p] = "-";
			if (sv[4] != MISSING_DATA_SIGN)
				PMatPrintSD.UpperCI[p] = AnsiString(sv[4].c_str()).ToDouble();
			else
				PMatPrintSD.UpperCI[p] = -1;
			if (sv[5] != MISSING_DATA_SIGN)
				PMatPrintSD.UpperCIWarning[p] = sv[5];
			else
				PMatPrintSD.UpperCIWarning[p] = "-";
		}
		PMatPrintIT.BestValue.resize(gutsPARAMETER_NO);
		PMatPrintIT.LowerCI.resize(gutsPARAMETER_NO);
		PMatPrintIT.LowerCIWarning.resize(gutsPARAMETER_NO);
		PMatPrintIT.UpperCI.resize(gutsPARAMETER_NO);
		PMatPrintIT.UpperCIWarning.resize(gutsPARAMETER_NO);
		PMatPrintIT.LowerEdgeJoint95.resize(gutsPARAMETER_NO);
		PMatPrintIT.UpperEdgeJoint95.resize(gutsPARAMETER_NO);
		PMatPrintIT.Broken.resize(gutsPARAMETER_NO);
		GetNoCommentLine(ifile, s);
		// "Parameter\tValue\tLower CI\tLower CI warning\tUpper CI\tUpper CI warning"
		for (int p = 0; p < gutsPARAMETER_NO; p++)
		{
			GetNoCommentLine(ifile, s);
			sv = SplitString(s, "\t");
			if (sv[1] != MISSING_DATA_SIGN)
				PMatPrintIT.BestValue[p] = AnsiString(sv[1].c_str()).ToDouble();
			else
				PMatPrintIT.BestValue[p] = -1;
			if (sv[2] != MISSING_DATA_SIGN)
				PMatPrintIT.LowerCI[p] = AnsiString(sv[2].c_str()).ToDouble();
			else
				PMatPrintIT.LowerCI[p] = -1;
			if (sv[3] != MISSING_DATA_SIGN)
				PMatPrintIT.LowerCIWarning[p] = sv[3];
			else
				PMatPrintIT.LowerCIWarning[p] = "-";
			if (sv[4] != MISSING_DATA_SIGN)
				PMatPrintIT.UpperCI[p] = AnsiString(sv[4].c_str()).ToDouble();
			else
				PMatPrintIT.UpperCI[p] = -1;
			if (sv[5] != MISSING_DATA_SIGN)
				PMatPrintIT.UpperCIWarning[p] = sv[5];
			else
				PMatPrintIT.UpperCIWarning[p] = "-";
		}
		// adjust values of PMatSD and PMatIT if possible
		PMatSD.Value = PMatPrintSD.BestValue;
		PMatIT.Value = PMatPrintIT.BestValue;
	}
	else
	{
		PMatSD.Value = PMatSD.Max;
		PMatIT.Value = PMatIT.Max;
	}

	// display the parameters in the GUI
	bool change_value = true;
	bool change_range = true;
	DisplayParameterMatrixInGUI(PMatSD, gutsSD, change_value, change_range);
	DisplayParameterMatrixInGUI(PMatIT, gutsIT, change_value, change_range);

	// ################ VALIDATION DATA ###################
	if (!WriteConcSurvInputFileToGUI(ifile, ValidationDataDescriptionRichEdit, ValidationSurvStringGrid, ValidationConcStringGrid,
		ConcUnitEdit, ValidControlCombo))
		return false;

	// ################ PREDICTIONS DATA ###################
	if (!WritePredictionsConcDataToGUI(ifile, PredictionsConcStringGrid))
		return false;

	// ############## PARAMETER SPACES ###################

	GetNoCommentLine(ifile, s); // "Number of parameter sets for SD:\t"
	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	NumberOfParameterSamplesSD = AnsiString(sv[sv.size() - 1].c_str()).ToInt();
	GetNoCommentLine(ifile, s);
	// "Parameter set\tkd\tmw\thb\tbw\tFs\tlog-likelihood"
	CalibrationParameterSpaceSD.resize(0);
	for (int ns = 0; ns < NumberOfParameterSamplesSD; ns++)
	{
		GetNoCommentLine(ifile, s);
		sv = SplitString(s, "\t");
		vector<double>param_temp;
		param_temp.resize(0);
		for (int p = 1; p < sv.size() - 1; p++)
			param_temp.push_back(AnsiString(sv[p].c_str()).ToDouble());
		CalibrationParameterSpaceSD.resize(CalibrationParameterSpaceSD.size() + 1);
		CalibrationParameterSpaceSD[CalibrationParameterSpaceSD.size() - 1].Parameter = param_temp;
		CalibrationParameterSpaceSD[CalibrationParameterSpaceSD.size() - 1].Likelihood =
			AnsiString(sv[sv.size() - 1].c_str()).ToDouble();
	}

	GetNoCommentLine(ifile, s); // "Number of parameter sets for IT:\t"
	sv = SplitString(s, ":");
	EraseSpaces(sv[sv.size() - 1]);
	NumberOfParameterSamplesIT = AnsiString(sv[sv.size() - 1].c_str()).ToInt();
	GetNoCommentLine(ifile, s);
	// "Parameter set\tkd\tmw\thb\tbw\tFs\tlog-likelihood"
	CalibrationParameterSpaceIT.resize(0);
	for (int ns = 0; ns < NumberOfParameterSamplesIT; ns++)
	{
		GetNoCommentLine(ifile, s);
		sv = SplitString(s, "\t");
		vector<double>param_temp;
		param_temp.resize(0);
		for (int p = 1; p < sv.size() - 1; p++)
			param_temp.push_back(AnsiString(sv[p].c_str()).ToDouble());
		CalibrationParameterSpaceIT.resize(CalibrationParameterSpaceIT.size() + 1);
		CalibrationParameterSpaceIT[CalibrationParameterSpaceIT.size() - 1].Parameter = param_temp;
		CalibrationParameterSpaceIT[CalibrationParameterSpaceIT.size() - 1].Likelihood =
			AnsiString(sv[sv.size() - 1].c_str()).ToDouble();
	}

	ifile.close();
	Screen->Cursor = crArrow;

	return true;
}
// ---------------------------------------------------------------------------

vector<string>TMainForm::SplitString(string s, string delimiter)
{
	int string_pos;
	vector<string>sv;
	sv.clear();

	// split a string
	while ((string_pos = s.find(delimiter)) != string::npos)
		// find first occurence of delimiter in string
	{
		sv.push_back(s.substr(0, string_pos));
		// push back substring until delimiter is reached
		s.erase(0, string_pos + delimiter.length());
		// erase the pushed back substring from the string
	}
	sv.push_back(s); // push back last segment of the string

	return sv;
}
// ---------------------------------------------------------------------------

void TMainForm::GetNoCommentLine(ifstream &ifile, string &s)
{
	getline(ifile, s);
	while (s[0] == '#')
		getline(ifile, s);

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::EraseSpaces(string &s)
{
	int i = 0;
	while (s[i])
	{
		if (isspace(s[i]))
		{
			s.erase(i, i + 1);
			i--;
		}
		i++;
	}

	return;
}
// ---------------------------------------------------------------------------

bool TMainForm::FillGridWithGridData(TGridData &grid_data, TStringGrid *grid)
{
	// first clear grid
	for (int r = 0; r < grid->RowCount; r++)
		for (int c = 0; c < grid->ColCount; c++)
			grid->Cells[c][r] = "";

	// then adjust size of grid if neccessary
	if (grid->ColCount < grid_data.GroupNames.size() + 1)
		grid->ColCount = grid_data.GroupNames.size() + 1;
	// number of groups + time column
	if (grid->RowCount < grid_data.Times.size() + 1)
		grid->RowCount = grid_data.Times.size() + 1; // number of times + header

	// now fill grid with data
	AnsiString as = "Times [" + AnsiString(grid_data.TimeUnit.c_str()) + "]";
	grid->Cells[0][0] = as.c_str();
	for (int r = 1; r < grid_data.Times.size() + 1; r++)
		grid->Cells[0][r] = grid_data.Times[r - 1];
	for (int c = 1; c < grid_data.GroupNames.size() + 1; c++)
		grid->Cells[c][0] = grid_data.GroupNames[c - 1];
	for (int r = 1; r < grid_data.Times.size() + 1; r++)
		for (int c = 1; c < grid_data.GroupNames.size() + 1; c++)
		{
			if (grid_data.Values[c - 1][r - 1] == MISSING_DATA_NUM)
				grid->Cells[c][r] = MISSING_DATA_SIGN.c_str();
			else
				grid->Cells[c][r] = grid_data.Values[c - 1][r - 1];
		}

	return true;
}

// ---------------------------------------------------------------------------
void __fastcall TMainForm::CalibDataOpenSpeed1Click(TObject *Sender)
{
	if (InputFileOpenDialog->Execute())
	{
		OpenConcSurvInputDataFile(AnsiString(InputFileOpenDialog->FileName).c_str(), InputDataDescriptionRichEdit1,
				CalibSurvStringGrid1, CalibConcStringGrid1, ConcUnitEdit, CalibControlCombo1);
		CalibrationDataFileNames[0] = AnsiString(InputFileOpenDialog->FileName);
		CalibrationPerformed = false;
		bool reset_calibration_input = false;
		Reset(reset_calibration_input);
	}
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::CalibDataSaveSpeed1Click(TObject *Sender)
{
	if (InputFileSaveDialog->Execute())
		SaveConcSurvInputDataFile(AnsiString(InputFileSaveDialog->FileName).c_str(), InputDataDescriptionRichEdit1,
		CalibSurvStringGrid1, CalibConcStringGrid1, ConcUnitEdit);
}
// ---------------------------------------------------------------------------

bool TMainForm::OpenConcSurvInputDataFile(string filename, TRichEdit *description, TStringGrid *surv_grid, TStringGrid *conc_grid,
	TEdit *conc_unit, TComboBox *combo)
{
	Screen->Cursor = crHourGlass;
	ifstream ifile(filename.c_str());

	if (!WriteConcSurvInputFileToGUI(ifile, description, surv_grid, conc_grid, conc_unit, combo))
	{
		Screen->Cursor = crDefault;
		return false;
	}

	ifile.close();
	ProjectSaved = false;
	Screen->Cursor = crDefault;

	return true;
}
// ---------------------------------------------------------------------------

bool TMainForm::WriteConcSurvInputFileToGUI(ifstream &ifile, TRichEdit *description, TStringGrid *surv_grid,
	TStringGrid *conc_grid, TEdit *conc_unit, TComboBox *combo)
{
	// first clear grids and description
	ClearGrid(surv_grid);
	ClearGrid(conc_grid);
	description->Clear();

	string s;
	vector<string>sv;
	int grid_row;

	// search header of survival grid and write survival data until "Concentration unit:" reached
	// every line before the survival grid information belongs to the description
	GetNoCommentLine(ifile, s);
	if (s == "No data in grid")
		// survival grid not filled --> concentration grid does not contain data too
	{
		GetNoCommentLine(ifile, s);
		GetNoCommentLine(ifile, s);
		// we have to skip the next two lines too (to skip missing conc and conc grid)
		return true; // it is ok, if there is no data available
	}
	else
		ValidationDataPresent = true;
	while (s.substr(0, 18) != "Survival time [d]\t")
	{
		description->Lines->Add(s.c_str());
		GetNoCommentLine(ifile, s);
		if (ifile.eof())
		{
			ShowMessage("Identifying header 'Survival time [d]' (followed by tab) not found!");
            ClearGrid(surv_grid);
			ClearGrid(conc_grid);
			description->Clear();
			SetDefaultHeader(surv_grid, gutsSURVIVAL);
			SetDefaultHeader(conc_grid, gutsEXPOSURE);
			return false;
		}
	}
	grid_row = 0;

	while (s.substr(0, 19) != "Concentration unit:")
	{
		sv = SplitString(s, "\t");
		// adjust grid size if neccessary
		if (surv_grid->ColCount < sv.size() + 10)
			surv_grid->ColCount = sv.size() + 10;
		if (surv_grid->RowCount < grid_row + 10)
			surv_grid->RowCount = grid_row + 10;
		for (int c = 0; c < sv.size(); c++)
			surv_grid->Cells[c][grid_row] = sv[c].c_str();
		grid_row++;
		GetNoCommentLine(ifile, s);
		if (ifile.eof())
		{
			ShowMessage("Identifying header 'Concentration unit:' (followed by tab) not found!");
			ClearGrid(surv_grid);
			ClearGrid(conc_grid);
			description->Clear();
			SetDefaultHeader(surv_grid, gutsSURVIVAL);
			SetDefaultHeader(conc_grid, gutsEXPOSURE);
			return false;
		}
	}

	UpdateControlGroupCombo(combo, surv_grid);

	sv = SplitString(s, ":"); // get concentration unit
	EraseSpaces(sv[sv.size() - 1]);
	string conc_str = sv[sv.size() - 1];
	if (conc_unit->Text != "" && conc_unit->Text != AnsiString(conc_str.c_str()))
		ShowMessage(
		"Caution: Concentration unit changed with loading a data set.\nEnsure that all data sets have the same concentration unit!");
	conc_unit->Text = conc_str.c_str();

	GetNoCommentLine(ifile, s);
	while (s.substr(0, 23) != "Concentration time [d]\t")
	{
		GetNoCommentLine(ifile, s);
		if (ifile.eof())
		{
			ShowMessage("Identifying header 'Concentration time [d]' (followed by tab) not found!");
			ClearGrid(surv_grid);
			ClearGrid(conc_grid);
			description->Clear();
			SetDefaultHeader(surv_grid, gutsSURVIVAL);
			SetDefaultHeader(conc_grid, gutsEXPOSURE);
			return false;
		}
	}
	grid_row = 0;
	sv = SplitString(s, "\t");
	// adjust grid size if neccessary
	if (conc_grid->ColCount < sv.size() + 10)
		conc_grid->ColCount = sv.size() + 10;
	for (int c = 0; c < sv.size(); c++)
		conc_grid->Cells[c][grid_row] = sv[c].c_str();
	grid_row++;
	getline(ifile, s);
	while (ifile)
	{
		if (s[0] == '#')
			break;
		sv = SplitString(s, "\t");
		// adjust grid size if neccessary
		if (conc_grid->ColCount < sv.size() + 10)
			conc_grid->ColCount = sv.size() + 10;
		if (conc_grid->RowCount < grid_row + 10)
			conc_grid->RowCount = grid_row + 10;
		for (int c = 0; c < sv.size(); c++)
			conc_grid->Cells[c][grid_row] = sv[c].c_str();
		grid_row++;
		getline(ifile, s);
		// normal getline to escape the while loop by finding comment line
	}

	// shorten the header for grid[0][0]
	surv_grid->Cells[0][0] = "Surv. time [d]";
	conc_grid->Cells[0][0] = "Conc. time [d]";

	return true;
}
// ---------------------------------------------------------------------------

void TMainForm::UpdateControlGroupCombo(TComboBox *combo, TStringGrid *grid)
{
	int old_item_index = combo->ItemIndex;

	combo->Clear();
	for (int c = 1; c < grid->ColCount; c++)
	{
		if (grid->Cells[c][0] != "")
			combo->Items->Add(grid->Cells[c][0]);
	}
	combo->Items->Add(" <No control group>");

	if (old_item_index < combo->Items->Count && old_item_index >= 0)
		combo->ItemIndex = old_item_index;
	else
		combo->ItemIndex = 0;

	return;
}
// ---------------------------------------------------------------------------

bool TMainForm::WritePredictionsConcDataToGUI(ifstream &ifile, TStringGrid *conc_grid)
{
	string s;
	vector<string>sv;

	GetNoCommentLine(ifile, s);

	PredictionsDataPresent = true;
	int grid_row = conc_grid->FixedRows;
	while (ifile)
	{
		if (s[0] == '#' || s == "No data in grid")
		{
			PredictionsDataPresent = false;
			break;
		}
		sv = SplitString(s, "\t");
		if (sv.size() > 2)
			return false;
		// adjust grid size if neccessary
		if (conc_grid->RowCount < grid_row + 10)
			conc_grid->RowCount = grid_row + 10;
		for (int c = 0; c < sv.size(); c++)
			conc_grid->Cells[c][grid_row] = sv[c].c_str();
		grid_row++;
		getline(ifile, s);
		// normal getline to escape the while loop by finding comment line
	}

	return true;
}
// ---------------------------------------------------------------------------

bool TMainForm::WriteBatchPredictionsConcDataToGUI(ifstream &ifile)
{
	string s;
	bool in_LPx_table = false;
	GetNoCommentLine(ifile, s);
    BatchPredictionDlg->FileListBox->Items->Clear();
	BatchPredictionDlg->BatchMemo->Lines->Clear();
	LPxBatch_Samplesize_Info_out.clear();
	LPxBatch_out.clear();
	while (ifile)
	{
		if (s[0] == '#' || s == "No data in grid")
		{
			break;
		}
		BatchPredictionDlg->BatchMemo->Lines->Add(s.c_str());
		if(s.substr(0,4) == "File") in_LPx_table = true;
		if (in_LPx_table)
		{
			LPxBatch_out.push_back(s.c_str());
		}
		else
		{
			LPxBatch_Samplesize_Info_out.push_back(s.c_str());
        }
		getline(ifile, s);
	}

	return true;
}

// ---------------------------------------------------------------------------

void TMainForm::ClearGrid(TStringGrid *grid)
{
	for (int c = grid->FixedCols; c < grid->ColCount; c++)
		for (int r = grid->FixedRows; r < grid->RowCount; r++)
		{
			grid->Cells[c][r] = "";
		}

	return;
}
// ---------------------------------------------------------------------------

bool TMainForm::SaveConcSurvInputDataFile(string filename, TRichEdit *description, TStringGrid *surv_grid, TStringGrid *conc_grid,
	TEdit *conc_unit)
{
	Screen->Cursor = crHourGlass;
	ofstream ofile(filename.c_str());

	// write description to file
	ofile << AnsiString(description->Lines->Text);

	// write survival grid to file
	WriteGridDataToOStream(ofile, surv_grid);

	// write concentration unit to file
	ofile << "Concentration unit:\t" << AnsiString(conc_unit->Text) << endl;

	// write concentration grid to file
	WriteGridDataToOStream(ofile, conc_grid);

	ofile.close();
	Screen->Cursor = crArrow;

	return true;
}
// ---------------------------------------------------------------------------

void TMainForm::WriteGridDataToOStream(ofstream &ofile, TStringGrid *grid)
{
	// first get dimension of data in grid (only filled cells of first row and column are considered)
	int number_of_rows = grid->FixedRows;
	int number_of_columns = grid->FixedCols;
	for (int r = grid->RowCount - 1; r >= grid->FixedRows; r--)
		if (grid->Cells[0][r] != "")
		{
			number_of_rows = r + 1;
			break;
		}
	for (int c = grid->ColCount - 1; c >= grid->FixedCols; c--)
		if (grid->Cells[c][0] != "")
		{
			number_of_columns = c + 1;
			break;
		}

	// then write grid data to ofstream
	for (int r = grid->FixedRows; r < number_of_rows; r++)
	{
		for (int c = grid->FixedCols; c < number_of_columns; c++)
		{
			if (c == 0 && r == 0)
			{
				if (grid->Cells[c][r] == "Surv. time [d]")
				{
					ofile << "Survival time [d]";
					continue;
				}
				else if (grid->Cells[c][r] == "Conc. time [d]")
				{
					ofile << "Concentration time [d]";
					continue;
				}
				else
				{
					// do nothing
                }
			}

			if (c == grid->FixedCols)
				ofile << AnsiString(grid->Cells[c][r]);
			else
				ofile << "\t" << AnsiString(grid->Cells[c][r]);
		}
		ofile << endl;
	}

	if (number_of_columns == grid->FixedCols || number_of_rows == grid->FixedRows)
		ofile << "No data in grid" << endl;

	return;
}
// ---------------------------------------------------------------------------

vector<double>TMainForm::CalcWilsonScore(int n, int survivors)
	// n = initial number of individuals
{
	vector<double>score;
	score.resize(2);

	if (n == 0)
	{
		score[0] = 0.0;
		score[1] = 0.0;
		return score;
	}

	double z = 1.96; // critical value from standard normal distribution
	double p = (double)survivors / (double)n; // survivor ratio
	double a, b;

	a = (p + pow(z, 2.0) / 2.0 / n) / (1 + pow(z, 2.0) / n);
	b = z / (1 + pow(z, 2.0) / n) * sqrt(p * (1 - p) / n + pow(z, 2.0) * 0.25 / pow((double)n, 2.0));
	score[0] = a - b;
	score[1] = a + b;

	return score;
}

// ---------------------------------------------------------------------------
void __fastcall TMainForm::CalibDataOpenSpeed2Click(TObject *Sender)
{
	if (InputFileOpenDialog->Execute())
	{
		OpenConcSurvInputDataFile(AnsiString(InputFileOpenDialog->FileName).c_str(), InputDataDescriptionRichEdit2,
				CalibSurvStringGrid2, CalibConcStringGrid2, ConcUnitEdit, CalibControlCombo2);
		CalibrationDataFileNames[1] = AnsiString(InputFileOpenDialog->FileName);
		CalibrationPerformed = false;
        bool reset_calibration_input = false;
		Reset(reset_calibration_input);
	}
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::CalibDataOpenSpeed3Click(TObject *Sender)
{
	if (InputFileOpenDialog->Execute())
	{
		OpenConcSurvInputDataFile(AnsiString(InputFileOpenDialog->FileName).c_str(), InputDataDescriptionRichEdit3,
				CalibSurvStringGrid3, CalibConcStringGrid3, ConcUnitEdit, CalibControlCombo3);
		CalibrationDataFileNames[2] = AnsiString(InputFileOpenDialog->FileName);
		CalibrationPerformed = false;
        bool reset_calibration_input = false;
		Reset(reset_calibration_input);
	}
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::CalibDataOpenSpeed4Click(TObject *Sender)
{
	if (InputFileOpenDialog->Execute())
	{
		OpenConcSurvInputDataFile(AnsiString(InputFileOpenDialog->FileName).c_str(), InputDataDescriptionRichEdit4,
				CalibSurvStringGrid4, CalibConcStringGrid4, ConcUnitEdit, CalibControlCombo4);
		CalibrationDataFileNames[3] = AnsiString(InputFileOpenDialog->FileName);
		CalibrationPerformed = false;
        bool reset_calibration_input = false;
		Reset(reset_calibration_input);
	}
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::CalibDataOpenSpeed5Click(TObject *Sender)
{
	if (InputFileOpenDialog->Execute())
	{
		OpenConcSurvInputDataFile(AnsiString(InputFileOpenDialog->FileName).c_str(), InputDataDescriptionRichEdit5,
				CalibSurvStringGrid5, CalibConcStringGrid5, ConcUnitEdit, CalibControlCombo5);
		CalibrationDataFileNames[4] = AnsiString(InputFileOpenDialog->FileName);
		CalibrationPerformed = false;
        bool reset_calibration_input = false;
		Reset(reset_calibration_input);
	}
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::CalibDataSaveSpeed5Click(TObject *Sender)
{
	if (InputFileSaveDialog->Execute())
		SaveConcSurvInputDataFile(AnsiString(InputFileSaveDialog->FileName).c_str(), InputDataDescriptionRichEdit5,
		CalibSurvStringGrid5, CalibConcStringGrid5, ConcUnitEdit);
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::CalibDataSaveSpeed4Click(TObject *Sender)
{
	if (InputFileSaveDialog->Execute())
		SaveConcSurvInputDataFile(AnsiString(InputFileSaveDialog->FileName).c_str(), InputDataDescriptionRichEdit4,
		CalibSurvStringGrid4, CalibConcStringGrid4, ConcUnitEdit);
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::CalibDataSaveSpeed3Click(TObject *Sender)
{
	if (InputFileSaveDialog->Execute())
		SaveConcSurvInputDataFile(AnsiString(InputFileSaveDialog->FileName).c_str(), InputDataDescriptionRichEdit3,
		CalibSurvStringGrid3, CalibConcStringGrid3, ConcUnitEdit);
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::CalibDataSaveSpeed2Click(TObject *Sender)
{
	if (InputFileSaveDialog->Execute())
		SaveConcSurvInputDataFile(AnsiString(InputFileSaveDialog->FileName).c_str(), InputDataDescriptionRichEdit2,
		CalibSurvStringGrid2, CalibConcStringGrid2, ConcUnitEdit);
}
// ---------------------------------------------------------------------------

void TMainForm::Prepare_Data(TPreparedDataContainer &data_all, TPreparedDataContainer &data_hb,
		vector<TDataSet> data_set, int number_of_sets, bool prefit_hb)
{


	/* % Matlab Usage: [data_all] = prepare_data(data,flag_stage,corr_factor)
	 %
	 % This function translates the data sets entered into the internal format.
	 % Internally, we like to have a single structured object, with the
	 % treatments in separate matrices to allow missing data points, and to be
	 % able to make some preparatory calculations (such as the derivation of
	 % deaths-per-interval from the survivors). Also, some checks on the data
	 % set are performed. Note that there can be multiple data sets entered,
	 % which means that the input <data> is a cell array that can contain
	 % multiple data sets.
	 %
	 % Note: for all data types, multiple data sets are allowed here. However,
	 % for the software we agreed on only using multiple data sets for
	 % calibration and batch-processing of LPx.
	 %
	 % Inputs
	 % <data>        structured array with datasets as read from file in <load_sample> with
	 %               survivors, exposure scenarios, names and unit
	 % <flag_stage>  flag to indicate which stage of the analysis we are called from
	 % <corr_factor> can be used to correct exposure profiles for predictions to
	 %               the correct unit (only used when <flag_stage> = 3).
	 %
	 % Outputs
	 % <data_all>    A structured object (cell array) with the following fields:
	 %
	 % Once for the object:
	 % <data_all.hb_hat>   = background hazard rate for all controls together
	 % <data_all.nr_sets>  = number of data sets
	 % <data_all.nr_treat> = vector with number of treatments in each data set
	 %
	 % Per data set:
	 % <data_all.time>  = entire time vector for survival data in this data set
	 % <data_all.c_twa> = vector with time-weighted averages for all treatments
	 % <data_all.c_max> = vector with maximum concentration over all treatments
	 %
	 % Per treatment:
	 % <data_all.surv> = matrix with time, survivors, deaths, prob. surv., Wilson score (min and max)
	 % <data_all.scen> = events-time matrix with time, concentration and slope
	 % <data_all.name> = name of this treatment (string)
	 %
	 % Author     : Tjalling Jager */

	/* %% BLOCK 1. Initial things
	 % Set and check the concentration unit. Check that it is the same when
	 % multiple data sets are entered, and that it is the same for validation
	 % and calibration. */

	// DATA CHECKS ALREADY DONE in Check... functions

	// ADDING DUMMY SURVIVORS DONE ELSEWHERE

	/* % Next, this section creates some dummy survivor data. The survival
	 % data sets are used to shape the exposure profiles later in this
	 % function. The dummy data avoids having to modify the core of this
	 % function, and allows the exposure data to run through the code
	 % smoothly.
	 % Extract the exposure-scenario matrix from the input <data>, and
	 % create dummy survival data. Note that <load_data> already checks
	 % whether there are multiple treatments entered (there can be only
	 % one per data set).
	 for (int d=0; d<NumberOfCalibrationDataSets; d++) 		// run through all data sets
	 data{d}.surv = [0 10 ; data{d}.scen(end,1) 10];
	 % Create dummy survival data (this helps to be able to use the rest
	 % of the code in this function). Just 10 survivors at t=0 and at
	 % end of exposure scenario. */

	// %% BLOCK 2. Some general things to put in the output structures

	// data_all.nr_sets = NumberOfCalibrationDataSets; 	// store number of data sets in the structure
	// data_all.hb_hat  = -1; 		// initialise background hazard rate from controls as -1

	// Also prepare a structure for the control data, which will be fitted later
	// data_hb.nr_sets  = nr_sets; 		// store number of data sets in the structure
	// data_hb.nr_treat = zeros(1,nr_sets); 		// this will store number of control treatments per data set in this data set (vector with 0 or 1)
	int flag_ctrl_exist = 0;
	// this flag will be set to 1 when there is at least one control data set
	int flag_ctrl_mort = 0;
	// this flag will be set to 1 when there is at least one control with some mortality

	/* %% BLOCK 3. Prepare object with survival data and exposure scenarios
	 % Code below runs through the different data sets. It creates a complex
	 % structure <data_all> that contains all information for all data sets
	 % entered, including the exposure scenarios. The data matrices that are
	 % entered are split up in the individual treatments, and several columns
	 % are added with information derived from the data. */

	vector<vector<TPreparedTreatmentSurv> >surv_data;
	// data set with survival data
	surv_data.resize(data_set.size());
	vector<vector<TPreparedTreatmentConc> >conc_data;
	// data set with concentration data
	conc_data.resize(data_set.size());
	vector<vector<double> >conc_twa; // data set with twa concentrations
	conc_twa.resize(data_set.size());
	vector<vector<double> >conc_max; // data set with maximum concentrations
	conc_max.resize(data_set.size());
	vector<vector<AnsiString> >group_names;
	group_names.resize(data_set.size());
	vector<vector<TPreparedTreatmentSurv> >ctrl_surv_data;
	// data set with survival data of control groups (dynamic size)
	ctrl_surv_data.resize(1);
	vector<vector<TPreparedTreatmentConc> >ctrl_conc_data;
	// data set with concentration data of control groups (dynamic size)
	ctrl_conc_data.resize(1);

	int nr_treat; // number of treatments in this data set
	vector<double>t_surv;
	// complete time vector from survivor data set (vector)
	vector<vector<double> >s_surv; // survivors in the data set (matrix)
	vector<double>t_scen; // time vector from exposure profiles (vector)
	vector<vector<double> >s_scen;
	// exposure concentrations in the data set (matrix)

	for (int d = 0; d < number_of_sets; d++) // run through all data sets
	{
		// BLOCK 3.1. Initial things.
		nr_treat = data_set[d].Concentration.GroupNames.size();
		// number of treatments in this data set
		surv_data[d].resize(nr_treat);
		conc_data[d].resize(nr_treat);
		// Extract info from this survivors data set
		t_surv = data_set[d].Survival.Times;
		// complete time vector from survivor data set (vector)
		s_surv = data_set[d].Survival.Values;
		// survivors in the data set (matrix)
		// Extract info from this set of exposure scenarios
		t_scen = data_set[d].Concentration.Times;
		// time vector from exposure profiles (vector)
		s_scen = data_set[d].Survival.Values;
		// exposure concentrations in the data set (matrix)

		// store group names
		group_names[d] = data_set[d].Concentration.GroupNames;

		/* % Store some info for this data set already in the object: the list of
		 % identifiers, the time vector, and the number of treatments. */
		// data_all.name(d,1:nr_treat) = data{d}.name; 		// copy names to <data_all> (cell arrays of strings)
		// data_all.time{d}            = t_surv; 				// store entire time vector of this data set
		// data_all.nr_treat(d)        = nr_treat; 				// store number of treatments in this data set

		/* % BLOCK 3.2. Preliminary checks on the input data. Producing errors
		 % when things are wrong. Note that <load_data> has already performed
		 % a number of checks itself! */

		// DATA CHECKS ALREADY DONE in Check... functions

		// BLOCK 3.3. Split up the survivor data per treatment.
		for (int g = 0; g < data_set[d].Survival.GroupNames.size(); g++) // run through treatments in data set
		{
			/* % BLOCK 3.3.1. Remove NaNs, add columns with number of deaths and
			 % survival probabilities. */
			vector<double>t_ic;
			// copy time vector as well (as we'll remove NaNs)
			vector<double>s_ic; // vector with survivors for this treatment only
			vector<double>d_ic; // vector with deaths for this treatment only
			vector<double>ps_ic;
			// vector with survival probabilities for this treatment only
			vector<double>wilson_min; // Wilson score min
			vector<double>wilson_max; // Wilson score max
			vector<double>wilson_temp; // temp Wilson score min and max
			t_ic.resize(0);
			s_ic.resize(0);
			d_ic.resize(0);
			ps_ic.resize(0);
			wilson_min.resize(0);
			wilson_max.resize(0);
			wilson_temp.resize(0);

			for (int t = 0; t < data_set[d].Survival.Times.size(); t++)
				if (data_set[d].Survival.Values[g][t] != MISSING_DATA_NUM)
					// exclude missing data
				{
					s_ic.push_back(data_set[d].Survival.Values[g][t]);
					t_ic.push_back(data_set[d].Survival.Times[t]);
					if (t > 0)
						d_ic.push_back(s_ic[s_ic.size() - 2] - s_ic[s_ic.size() - 1]);
					// vector with deaths per interval (incl. the interval after the test)
					if (data_set[d].Survival.Values[g][0] > EPS)
						ps_ic.push_back(data_set[d].Survival.Values[g][t] / data_set[d].Survival.Values[g][0]);
					// vector with survival probabilities
					else
						ps_ic.push_back(0.0);
					if (ps_ic[ps_ic.size() - 1] < 0)
						// make sure there are no zombies in there ...
					{
						ShowMessage("Number of survivors should never increase in time, so check your data set.");
						return;
					}
					// BLOCK 3.3.2. Wilson score interval on data probabilities.
					wilson_temp = CalcWilsonScore(data_set[d].Survival.Values[g][0], data_set[d].Survival.Values[g][t]);
					wilson_min.push_back(wilson_temp[0]);
					wilson_max.push_back(wilson_temp[1]);
				}
			d_ic.push_back(s_ic[s_ic.size() - 1]);

			/* % BLOCK 3.3.3. Combine all vectors into the overall object, as a
			 % nice matrix for data set <i_d> and treatment <i_c>. columns:
			 % time, survivors, nr. deaths, surv. prob., CI min, CI max */
			surv_data[d][g].Times = t_ic;
			surv_data[d][g].Survivors = s_ic;
			surv_data[d][g].Deaths = d_ic;
			surv_data[d][g].SurvivalProb = ps_ic;
			surv_data[d][g].WilsonScoreMin = wilson_min;
			surv_data[d][g].WilsonScoreMax = wilson_max;
		}

		/* % BLOCK 3.4. Split up the exposure scenarios per treatment. Note: I run
		 % through the treatments again here. It could be done in the same loop
		 % as the survival data, but a separate loop may be more transparent.
		 % Note that <load_data> already performed checks whether all survival
		 % treatments are unique, and whether they all have matching exposure
		 % scenarios (and places these scenarios in the same order, if needed). */

		vector<double>c_twa;
		c_twa.resize(data_set[d].Concentration.GroupNames.size());
		// initialise vector to collect TWA concentrations
		vector<double>c_max;
		c_max.resize(data_set[d].Concentration.GroupNames.size());
		// initialise vector to collect maximum concentrations

		/* % BLOCK 3.4.1. Constant exposure can be entered as a single row. We'll
		 % add a final time point, so it will run smoothly through the same
		 % piece of code as for time-varying concentrations. */
		if (data_set[d].Concentration.Times.size() == 1)
			// if it is only a single row (constant)
		{
			// Add final time point with same concentrations.
			data_set[d].Concentration.Times.push_back(data_set[d].Survival.Times[data_set[d].Survival.Times.size() - 1]);
			// copy the scenario row vector to a second row
			for (int g = 0; g < data_set[d].Concentration.GroupNames.size(); g++)
				data_set[d].Concentration.Values[g].push_back(data_set[d].Concentration.Values[g][0]);
			// and add a final time point to the time vector
		}

		for (int g = 0; g < data_set[d].Concentration.GroupNames.size(); g++) // run through treatments of this data set again
		{
			/* % BLOCK 3.4.2. For each scenario, create an events matrix <ev_ic>
			 % with places where the linear patterns changes, and a slope in the
			 % third column. Start from the time-conc matrix for this scenario,
			 % but exclude last time point (things stay constant after last time
			 % point). Third column has the linear slopes in each interval. */

			TPreparedTreatmentConc ev_ic;
			vector<double>time;
			vector<double>conc;
			vector<double>slope;

			// first clean temporary data from missing data
			vector<double>clean_time = data_set[d].Concentration.Times;
			vector<double>clean_conc = data_set[d].Concentration.Values[g];
			for (int i = 0; i < clean_time.size(); i++)
				if (clean_conc[i] == MISSING_DATA_NUM)
				{
					clean_time.erase(clean_time.begin() + i);
					clean_conc.erase(clean_conc.begin() + i);
					i--;
				}

			// for t=0 manually to operate with t-1 in loop (first data rows cannot be zero)
			double x_diff, y_diff;
			int t0 = 0;
			while (t0 < clean_time.size() - 1 && clean_time[t0] == clean_time[t0 + 1])
				// skip duplicate time points at the beginning
					t0++;
			// first and last elements must be handled manually
			time.push_back(clean_time[t0]);
			conc.push_back(clean_conc[t0]);
			if (clean_conc.size() == 1)
				// if only one row of concentration exists
					slope.push_back(0.0);
			else
			{
				x_diff = clean_time[t0 + 1] - clean_time[t0];
				y_diff = clean_conc[t0 + 1] - clean_conc[t0];
				if (abs(x_diff) > 0)
					slope.push_back(y_diff / x_diff);
				else
				{
					if (y_diff > 0)
						slope.push_back(HIGH_POS_DOUBLE);
					else
						slope.push_back(HIGH_NEG_DOUBLE);
				}
				int last_skip_t = -1;
				for (int t = t0 + 1; t < clean_time.size() - 1; t++)
				{
					x_diff = clean_time[t + 1] - clean_time[t];
					if (x_diff == 0) // slope would cause INF
					{
						last_skip_t = t;
						continue;
					}

					y_diff = clean_conc[t + 1] - clean_conc[t];
					if (slope.size() > 0 && y_diff / x_diff == slope[slope.size() - 1] && t - 1 != last_skip_t)
						// if slope does not change (pruning) and last entry not skipped
							continue;

					time.push_back(clean_time[t]);
					conc.push_back(clean_conc[t]);
					if (abs(x_diff) > 0)
						slope.push_back(y_diff / x_diff);
					else
					{
						if (y_diff > 0)
							slope.push_back(HIGH_POS_DOUBLE);
						else
							slope.push_back(HIGH_NEG_DOUBLE);
					}
				}
				if (clean_conc[clean_time.size() - 1] != clean_conc[clean_time.size() - 2])
					// should last entry be added or not
				{
					time.push_back(clean_time[clean_time.size() - 1]);
					conc.push_back(clean_conc[clean_time.size() - 1]);
					slope.push_back(0.0); // last slope automatically zero
				}
				else
					slope[slope.size() - 1] = 0.0;
			}

			/* % Note: pruning needs to be done before removing any INFs, because
			 % we allow multiple entries at the same time point. We thus can
			 % have two intervals with the same slope that are not continuously
			 % increasing. Pruning is thereby not always 100% effective when
			 % there are such multiple timepoints (e.g., in the controls, there
			 % may be multiple time points, but the slope and concentration
			 % remain at zero; these are not recognised as 'prunable'). I don't
			 % think this leads to serious inefficiency in the calculations. */

			/* % Take care of cases where two instances of the same time point are
			 % entered (which signals that we want to have an instantaneous
			 % change in conc.). That gives a slope of infinity. */

			DetermineMaximumSurvivalTime(surv_data[d]);
			// Make sure that the exposure scenario nicely matches the survival data.
			while (time[time.size() - 1] > MaximumSurvivalTime)

				// does the exposure scenario extend beyond the time vector of the survival data?
				// But: consider all treatments when checking if there are later concentration time points than survival measurement time points
				// Else it could happen that we remove a concentration time point for which a survival time point exists in another treatment group
			{
				// remove any extra events
				time.resize(time.size() - 1);
				conc.resize(conc.size() - 1);
				slope.resize(slope.size() - 1);
			}
			// This could well leave <ev_ic> short, but that is fixed with the next step
			if (surv_data[d][g].Times[surv_data[d][g].Times.size() - 1] > time[time.size() - 1])
				// do we need more points for survival than available in the exposure scenario?
			{
				time.push_back(surv_data[d][g].Times[surv_data[d][g].Times.size() - 1]);
				conc.push_back(conc[conc.size() - 1] + slope[slope.size() - 1] * (time[time.size() - 1] - time[time.size() - 2]));
				slope.push_back(0.0);
			}
			/* % Add an extra time point in <ev_ic>, extrapolated from last
			 % one, and add slope zero. Note: when the scenario was short
			 % already at the input, the final slope is zero, so there is no
			 % extrapolation. There is only extrapolation with a non-zero
			 % slope if we cut off the exposure scenario in the previous if
			 % statement. */

			// store data in structure
			conc_data[d][g].Times = time;
			conc_data[d][g].Conc = conc;
			conc_data[d][g].Slope = slope;

			/* % BLOCK 3.4.3. Calculate the maximum and TWA concentration for this
			 % treatment. This is useful for scaling plots and for calculating
			 % the parameter ranges in <startgrid>. */
			c_twa[g] = 0.0;
			// initialise TWA as zero (as we'll continuously add to it)
			c_max[g] = conc_data[d][g].Conc[0];
			// start at start concentration for this treatment
			for (int t = 0; t < conc_data[d][g].Times.size() - 1; t++) // run through all time intervals
			{
				double delta_t = conc_data[d][g].Times[t + 1] - conc_data[d][g].Times[t]; // length of time interval
				double c_start = conc_data[d][g].Conc[t];
				// start concentration in this interval
				double slope = conc_data[d][g].Slope[t];
				// slope in this interval
				double c_end = c_start + slope * delta_t;
				// concentration at end of this interval
				if (conc_data[d][g].Times[conc_data[d][g].Times.size() - 1] > 0)
					c_twa[g] += 0.5 * (c_start + c_end) * delta_t / conc_data[d][g].Times[conc_data[d][g].Times.size() - 1];
				else
					c_twa[g] = HIGH_POS_DOUBLE;
				/* % Add contribution of this interval to the TWA. Contribution is
				 % mean of start and end concentration in the interval, weighted
				 % with the length of the interval, as fraction of the total
				 % length of the scenario. */
				c_max[g] = Max(c_max[g], c_end);
				// this will be the max exposure concentration across all time points
				/* % This is done here to make sure that we catch the max (which
				 % might be higher than one of the entries of ev_ic due to the
				 % possibility of two concentrations at the same time points
				 % where the latter is removed). */
			}
		}

		conc_twa[d] = c_twa;
		// store the time-weighted averages over all treatments
		conc_max[d] = c_max;
		// store the maximum concentration over all treatments

		/* % BLOCK 3.4. Also collect the survival data set for the control (where
		 % the scenario identifier is 'Control'). This fills a new data object <data_hb>
		 % for the control data so it can be easily fitted later on. */

		// Note: <strcmpi> compares strings without caring for case.
		TPreparedTreatmentConc conc_data_tmp;
		conc_data_tmp.Times.resize(2);
		conc_data_tmp.Conc.resize(2);
		conc_data_tmp.Slope.resize(2);
		conc_data_tmp.Times[0] = 0.0;
		conc_data_tmp.Times[1] = 0.0;
		// conc_data_tmp.Times[1] is adjusted in the following if statement
		conc_data_tmp.Conc[0] = 0.0;
		conc_data_tmp.Conc[1] = 0.0;
		conc_data_tmp.Slope[0] = 0.0;
		conc_data_tmp.Slope[1] = 0.0;
		if (data_set[d].ControlGroupIndex < data_set[d].Survival.GroupNames.size())
			// otherwise " <No control>" chosen in combo box
		{
			ctrl_surv_data[0].push_back(surv_data[d][data_set[d].ControlGroupIndex]);
			// [0] because there is only one data set consisting of all control groups over all data sets
			conc_data_tmp.Times[1] =
				surv_data[d][data_set[d].ControlGroupIndex].Times[surv_data[d][data_set[d].ControlGroupIndex].Times.size() - 1];
			ctrl_conc_data[0].push_back(conc_data_tmp);
			// set concentrations to zero, otherwise there could occur an error (division by zero) in calc_surv_sd_c()

			flag_ctrl_exist = 1;
			// flag that there is at least one control data set
			if (ctrl_surv_data[0][ctrl_surv_data[0].size() - 1].Survivors[0] != ctrl_surv_data[0][ctrl_surv_data[0].size() -
				1].Survivors[ctrl_surv_data[0][ctrl_surv_data[0].size() - 1].Survivors.size() - 1])
				// check if there is mortality in this control (compare first and last survivor number)
					flag_ctrl_mort = 1;
			// flag that there is control mortality, so we need fitting
		}
	}

	data_hb.Survival = ctrl_surv_data;
	data_hb.Concentration = ctrl_conc_data;

	data_all.GroupNames = group_names;

	/* %% BLOCK 4. Fit background mortality <hb> on all controls
	 % The structured object <data_hb> contains a sub-data set for the controls
	 % only. When there are no controls at all, the entry in the data object
	 % <data_all.hb_hat> will remain at -1, and <startgrid> will produce an
	 % error when the user requests fixing the background hazard to the
	 % controls. */

	vector<double>pfit;

	if (flag_ctrl_exist == 1) // if at least one control data set is located ...
	{
		double hb_min = 1e-6;
		// minimum value for hb (best have same as in startgrid!)
		// if there is no mortality in the controls at all, there is no need to fit:
		data_all.BackgroundHazard = hb_min;
		// store the minimum <hb> in the structure <data_all>
		/* % This avoids having a weird value for <hb> such as 2.476e-6 where
		 % 1e-6 would fit just as well. */
		if (flag_ctrl_mort != 0 && prefit_hb) // we need to fit!
		{
			// make a fake <pmat> with zeros
			TParameterMatrix pmat;
			pmat.Value.resize(gutsPARAMETER_NO);
			fill(pmat.Value.begin(), pmat.Value.end(), 0.0);
			pmat.Fit.resize(gutsPARAMETER_NO);
			fill(pmat.Fit.begin(), pmat.Fit.end(), gutsNOT_FIT_PARAM);
			pmat.Min.resize(gutsPARAMETER_NO);
			fill(pmat.Min.begin(), pmat.Min.end(), 0.0);
			pmat.Max.resize(gutsPARAMETER_NO);
			fill(pmat.Max.begin(), pmat.Max.end(), 0.0);
			pmat.Scale.resize(gutsPARAMETER_NO);
			fill(pmat.Scale.begin(), pmat.Scale.end(), gutsNORM_SCALE_PARAM);
			// but all on normal scale

			// prepare a row in <pmat> for <hb> (0.1 should be high enough as max)
			pmat.Value[gutsHB] = 0.01;
			pmat.Fit[gutsHB] = gutsFIT_PARAM;
			pmat.Min[gutsHB] = hb_min;
			pmat.Max[gutsHB] = 0.1;
			pfit.resize(1);
			pfit[0] = pmat.Value[gutsHB];
			// parameter vector to be fitted (only <hb>)

			// Use <fminsearch> and the standard <transfer> function to fit the control data.

			Setup_Transfer(pfit, pmat, gutsSD, data_hb);
			ERR = "";
			nelmin(Transfer_External, NelminN, NelminStart, NelminXmin, &NelminYnewlo, nelminSTD_REQMIN, NelminStep, nelminKONVGE,
				optimSTD_MAXFUNEVALS_PER_FITPARAM*NelminN, &NelminIcount, &NelminNumres, &NelminIfault);
			if (ERR != "")
				return;
			data_all.BackgroundHazard = NelminXmin[0];
			// store the best-fitting <hb> in the structure <data_all>
		}
	}

	// final data storage in suitable structure
	data_all.Survival = surv_data;
	data_all.Concentration = conc_data;
	data_all.TWAConcentration = conc_twa;
	data_all.MaxConcentration = conc_max;



	return;
}
// ---------------------------------------------------------------------------

void TMainForm::Calc_Surv_IT(vector<double> &Sc, double &LCx, double &LPx, vector<double>maxDw, vector<double>parameter_set,
	double t, double Feff, double mf_try)
{


	Application->ProcessMessages();
	if (StopBatchPredictions)
	{
		return;
	}

	/* % Matlab Usage: [Sc,LCx,LPx] = calc_surv_it(maxDw,pmat,t,Feff,mf_try)
	 %
	 % This function calculates survival probability <Sc> for the individual
	 % tolerance model (can be a vector). Additionally, it provides a direct
	 % calculation for LCx and LPx.
	 %
	 % Inputs
	 % <maxDw>   the maximum damage over time (can be a vector with moving max over time or single value)
	 % <pmat>    the total parameter matrix (or vector with first column only)
	 --> changed to vector<double> parameter_set
	 % <t>       the time at which to calculate the output (only used for LCx where it is a single value)
	 % <Feff>    the fraction effect (x/100 in LCx and LPx)
	 % <mf_try>  the multiplication factor to use to calculate survival (for use with <calc_lpx>)
	 %
	 % Outputs
	 % <Sc>      survival probability due to chemical stress for each element of <t>
	 % <LCx>     the LCx, calculated directly from the parameters in <pmat>
	 % <LPx>     the LPx, calculated directly from the parameters in <pmat> and <maxDw>
	 %
	 % Author: Tjalling Jager */

	double mw, Fs, beta, kd;

	// Extract parameters from <pmat>.
	mw = parameter_set[gutsMW]; // median of threshold distribution
	mw = Max(mw, 1.0e-100);
	// make sure that the threshold is not exactly zero ...;  Note: at this moment that is not needed as we set a non-zero lower bound for <mw>.
	Fs = parameter_set[gutsFS]; // fraction spread of the threshold distribution
	Fs = Max(1.0 + 1.0e-6, Fs); // make sure it is always slightly above 1
	if (abs(Fs - 1.0) > EPS)
		beta = log(39.0) / log(Fs);
	// shape parameter for log-logistic from <Fs>
	else
		beta = HIGH_POS_DOUBLE;
	kd = parameter_set[gutsKD]; // dominant rate constant

	if (mf_try < 0) // if it is empty ...
			mf_try = 1; // use 1 (which implies no multiplication of <maxDw>)

	// survival probability for log-logistic distribution
	if (maxDw.size() == 1)
	{
		Sc.resize(1);
		double basis;
		if (mw > EPS)
			basis = mf_try * maxDw[0] / mw;
		else
			basis = HIGH_POS_DOUBLE;
		if (basis == 0)
			// simplify Sc[i] calculation and catch log() error of the following 'else if'
				Sc[0] = 1.0;
		else if (beta * log(basis) < log(HIGH_POS_DOUBLE))
			// allows pow() calculation without overflow
				Sc[0] = 1.0 / (1.0 + pow(basis, beta)); // maxDW is single value
		else // if 'else if' is rejected pow() would return an overflow
				Sc[0] = 0.0; // set to zero due to overflow of pow()
	}
	else
	{
		Sc.resize(maxDw.size());
		for (int i = 0; i < Sc.size(); i++)
		{
			double basis;
			if (mw > 0)
				basis = mf_try * maxDw[i] / mw;
			else
				basis = HIGH_POS_DOUBLE;
			if (basis == 0)
				// simplify Sc[i] calculation and catch log() error of the following 'else if'
					Sc[i] = 1.0;
			else if (beta * log(basis) < log(HIGH_POS_DOUBLE))
				// allows pow() calculation without overflow
					Sc[i] = 1.0 / (1.0 + pow(basis, beta));
			// maxDW is vector with same size as Sc
			else // if 'else if' is rejected pow() would return an overflow
					Sc[i] = 0.0; // set to zero due to overflow of pow()
		}
	}

	/* % Note: in this calculation, <maxDw> can be a vector (then, <Sc> will also
	 % be a vector). Also note that <mf_try> is in here as <calc_lpx> also wants
	 % to calculate survival at a range of multiplication factors. */

	LCx = -1.0;
	LPx = -1.0;
	if (Feff > -1)
		// no need to calculate anything more when <Feff> is empty (that may lead to error); I am pretty sure that in the next calculations, all variables are always single values
	{
		if (mw == 0.0)
		{
			LCx = 0.0;
			LPx = 0.0;
		}
		else if (1.0 / beta * log(Feff / (1.0 - Feff)) < log(HIGH_POS_DOUBLE))
			// avoiding pow overflow and dividing by zero
		{
			if (kd > EPS && t > EPS && abs(Feff - 1.0) > EPS && beta > EPS)
				LCx = (mw / (1.0 - exp(-kd * t))) * pow(Feff / (1.0 - Feff), 1.0 / beta); // calculate LCx
			else
				LCx = HIGH_POS_DOUBLE;
			if (maxDw[0] > EPS && abs(Feff - 1.0) > EPS && beta > EPS)
				LPx = (mw / maxDw[0]) * pow(Feff / (1.0 - Feff), 1.0 / beta);
			// calculate LPx; maxDw should only have one element
			else
				LPx = HIGH_POS_DOUBLE;
		}
		else
		{
			LCx = HIGH_POS_DOUBLE;
			LPx = HIGH_POS_DOUBLE;
		}
	}



	return;
}
// ---------------------------------------------------------------------------

void TMainForm::Calc_Surv_IT(vector<double> &Sc, vector<double>maxDw, vector<double>parameter_set)
{
	// function call without t (empty), Feff (empty), mf_try (empty), LCx and LPx
	double f1, f2, f3, f4, f5;
	f1 = -1;
	f2 = -1;
	f3 = -1;
	f4 = -1;
	f5 = -1;

	Calc_Surv_IT(Sc, f1, f2, maxDw, parameter_set, f3, f4, f5);

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::Calc_Surv_IT(vector<double> &Sc, double &LCx, double &LPx, vector<double>maxDw, vector<double>parameter_set,
	double t, double Feff)
{
	// function call without mf_try (empty)
	Calc_Surv_IT(Sc, LCx, LPx, maxDw, parameter_set, t, Feff, -1);

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::Calc_Surv_SD_c(double &crit, vector<double> &Sc, double conc,
	vector<double>parameter_set, vector<double>t, double Feff, AnsiString &err)
{


    Application->ProcessMessages();
	if (StopBatchPredictions)
	{
		return;
	}


	// if arguments crit or Feff should not be used

	/* % Matlab Usage: [crit,Sc] = calc_surv_sd_c(c,pmat,t,Feff)
	 %
	 % This function calculates survival probability <Sc> under stochastic death
	 % when exposure is constant. Additionally, it provides a criterion <crit>
	 % to be used by fzero to calculate the LCx for SD. As LCx is defines as x%
	 % effect under constant exposure, we can use the analytical solution here
	 % as well (and ignore background hazard). The <c> needs to be the first
	 % input parameter as that is what <fzero> will be able to change. Also for
	 % <fzero>, <crit> needs to be the first output.
	 %
	 % Inputs:
	 % <c>       the concentration at which to calculate the output
	 % <pmat>    the parameter matrix (or first column only)
	 --> changed to vector<double> parameter_set
	 % <t>       the time at which to calculate the output (can be a vector)
	 % <Feff>    the fraction effect (x/100 in LCx)
	 %
	 % Outputs
	 % <crit>      criterion that is zero when survival at end is 1-x/100
	 % <Sc>        survival probability over time <t>, due to the chemical
	 %
	 % Author: Tjalling Jager */

	double mw, bw, kd;
	vector<double>minH;
	minH.resize(t.size());
	Sc.resize(minH.size());

	// Extract parameters from PMatSD
	mw = parameter_set[gutsMW]; // median of threshold distribution
	bw = parameter_set[gutsBW]; // killing rate
	kd = parameter_set[gutsKD]; // dominant rate constant

	// For constant exposure, we can use analytical solutions for SD.
	for (int i = 0; i < minH.size(); i++)
		minH[i] = 0;

	double t0;
	if (conc > mw) // if concentration to test is above the threshold ...
	{
		if (conc > EPS && kd > EPS)
			t0 = -log(1 - mw / conc) / kd;
		// no-effect-time: time point where effects start
		else
			t0 = HIGH_POS_DOUBLE;
	}

	for (int i = 0; i < minH.size(); i++)
	{
		if (conc > mw) // if concentration to test is above the threshold ...
				minH[i] = (bw / kd) * Max(0.0, exp(-kd * t0) - exp(-kd * t[i])) * conc - bw * (Max(0.0, conc - mw)) * Max(0.0,
			t[i] - t0);
		// replace <minH> by the calculated effects over time (vector)
		Sc[i] = Min(1.0, exp(minH[i]));
		// turn <minH> into survival probability (vector)
	}

	crit = -1.0;
	if (Feff > -1)
		// no need to calculate anything else than <survival_prob> if Feff == -1
	{
		Sc[0] = Sc[Sc.size() - 1];
		// only return survival at the end of the exposure profile
		Sc.resize(1);
		crit = Sc[0] - (1.0 - Feff);
		// calculate criterion: zero when end value is x*100% effect (for <fzero> to work on)
	}



	return;
}
// ---------------------------------------------------------------------------

void TMainForm::Calc_Surv_SD_c(vector<double> &Sc, double conc, vector<double>parameter_set, vector<double>t, AnsiString &err)
{
	// if function call without fraction effect, set fraction effect to -1
	double f1, f2;
	f1 = -1;
	f2 = -1;

	Calc_Surv_SD_c(f1, Sc, conc, parameter_set, t, f2, err);

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::Calc_Surv_SD_t(double &crit, vector<double> &Sc, double mf_try, vector<double>parameter_set, vector<double>t,
	vector<double>Dw, double Feff, AnsiString &err)
{


    Application->ProcessMessages();
	if (StopBatchPredictions)
	{
		return;
	}


	/* % Matlab Usage: [crit,Sc] = calc_surv_sd_t(mf_try,pmat,t,Dw,Feff)
	 %
	 % This function calculates survival probability <Sc> under stochastic death
	 % when exposure is time-varying, using numerical integration with the
	 % trapezium rule. Additionally, it provides a criterion <crit> to be used
	 % by <fzero> to calculate the LPx. LPx is defined as the factor by which to
	 % multiply an exposure profile to obtain x% effect at the end of the
	 % profile. I here make use of the fact that multiplying a profile by a
	 % factor is equivalent to multiplying damage levels by that factor. This
	 % works as long as damage is a simple one-compartment thing! The <mf_try>
	 % needs to be the first input parameter as that is what <fzero> will be
	 % able to change. Also for <fzero>, <crit> needs to be the first output.
	 %
	 % Inputs
	 % <mf_try>    the multiplication factor to try
	 % <pmat>      the parameter matrix (or only the first column)
	 --> changed to vector<double> parameter_set
	 % <t>         the time vector at which to calculate the output (should match <Dw>)
	 % <Dw>        damage (as vector over time <t>)
	 % <Feff>      the fraction effect (x/100 in LPx); when not empty, only
	 %             return <Sc> at the end of the time vector (LPx calculations)
	 %
	 % Outputs
	 % <crit>      criterion that is zero when survival at end is 1-x/100
	 % <Sc>        survival probability over time <t>, due to the chemical
	 %
	 % Author: Tjalling Jager */

	if (t.size() != Dw.size())
	{
		err = "Time vector and damage vector have to be of same length!\nError in Calc_Surv_SD_t()!";

		return;
	}

	double mw, bw;
	vector<double>hc;
	hc.resize(Dw.size());
	vector<double>cum_hc;
	Sc.resize(Dw.size());

	// Extract parameters from <PMatSD>.
	mw = parameter_set[gutsMW]; // median of threshold distribution
	bw = parameter_set[gutsBW]; // killing rate

	for (int i = 0; i < Dw.size(); i++)
	{
		Dw[i] *= mf_try; // use multiplication on damage
		hc[i] = bw * Max(0.0, Dw[i] - mw);
		// calculate hazard for each time point
	}
	cum_hc = Cumtrapz(t, hc);
	// integrate the hazard rate numerically over the time vector
	for (int i = 0; i < Dw.size(); i++)
		Sc[i] = Min(1.0, exp(-1.0 * cum_hc[i]));
	// calculate survival probability due to chemical stress

	crit = -1.0;
	if (Feff > -1) // no need to calculate anything else but <Sc> if Feff == -1
	{
		Sc[0] = Sc[Sc.size() - 1];
		// only return survival at the end of the exposure profile
		Sc.resize(1);
		crit = Sc[0] - (1.0 - Feff);
		// calculate criterion: zero when end value is x*100% effect
	}



	return;
}
// ---------------------------------------------------------------------------

void TMainForm::Calc_Surv_SD_t(vector<double> &Sc, double mf_try, vector<double>parameter_set, vector<double>t, vector<double>Dw, AnsiString &err)
{
	// if function call without fraction effect, set fraction effect to -1
	double f1, f2;
	f1 = -1;
	f2 = -1;

	Calc_Surv_SD_t(f1, Sc, mf_try, parameter_set, t, Dw, f2, err);

	return;
}
// ---------------------------------------------------------------------------

vector<double>TMainForm::Cumtrapz(vector<double>&x, vector<double>&y)
{
	double x_diff, y_mean;
	vector<double>cum;
	cum.resize(x.size());

	if (x.size() != y.size())
	{
		ShowMessage("Error in Cumtrapz(): input vectors are not allowed to differ in length!");
		return cum;
	}

	cum[0] = 0.0;
	for (int i = 1; i < x.size(); i++)
	{
		x_diff = x[i] - x[i - 1];
		y_mean = 0.5 * (y[i] + y[i - 1]);
		cum[i] = cum[i - 1] + x_diff * y_mean;
	}

	return cum;
}
// ---------------------------------------------------------------------------


vector<double>TMainForm::Calc_Damage_Linear(vector<double> &t, double kd, TPreparedTreatmentConc ev_ic)
{


	/* % Matlab Usage: Dw = calc_damage_linear(t,kd,ev_ic)
	 %
	 % This function calculates scaled damage for the reduced GUTS model system,
	 % given a series of linearly-changing exposure concentrations. Note that
	 % the survival probability due to chemical stress is calculated separately.
	 % This function sequentially applies the analytical solution for scaled
	 % damage, for each episode between events. This function is only called
	 % from <calc_model>.
	 %
	 % Inputs
	 % <t>      the time vector
	 % <kd>     the dominant rate constant
	 % <ev_ic>  the exposure-events matrix for a single treatment (three columns)
	 %
	 % Output
	 % <Dw>     vector with scaled damage at each value of <t>
	 %
	 % Author: Tjalling Jager */

	vector<double>Dw; // vector to return
	vector<int>ind_t; // logical index for the period between two events
	vector<double>te; // new part of time vector
	double c_start; // initial concentration
	double slope; // slope of concentration at a certain time point
	double t_end; // last time point in an interval

	Dw.resize(t.size(), 0.0); // initialise output vector for damage with NaNs

	double Dw0 = 0; // make initial damage level zero
	Dw[0] = Dw0; // and put it in the first spot of the output vector


	for (int i = 0; i < ev_ic.Conc.size() - 1;
	i++) // run through all events (not last one, which marks the end of the scenario, to which we extrapolate with the slope)
	{
		c_start = ev_ic.Conc[i];
		// take as initial concentration the new one in this period
		slope = ev_ic.Slope[i]; // take as slope the new one in this period

		for (int j = 0; j < t.size(); j++)
		{
			// Calculate damage for all time slices
			if (t[j] > ev_ic.Times[i] && t[j] <= ev_ic.Times[i + 1] + EPS)
			// find the logical indices for the period between two events
			{
				// Thanks to Bob Kooi for using Maple to obtain the solution for me.
				if (kd > 1.0E-100)
				{
					Dw[j] = slope * (t[j] - ev_ic.Times[i]) + c_start - slope / kd + exp(-kd * (t[j] - ev_ic.Times[i])) *
					(Dw0 - c_start + slope / kd);
				}
				else
				{
					Dw[j] = 0.0;
				// 'te' of original code replaced by (t[j] - ev_ic.Times[i]) to directly calculate Dw
				}
			}
else
	int test=0;
		}

		// Find new starting values at exact moment of start of new period.
		t_end = ev_ic.Times[i + 1] - ev_ic.Times[i];

		// last time point in this interval is start time for new period
		if (kd > 1.0E-100)
		{
			Dw0 = slope * t_end + c_start - slope / kd + exp(-kd * t_end) * (Dw0 - c_start + slope / kd);
		}
		else
		{
			Dw0 = 0.0;
		}
		// % No need to do this in the last iteration of the loop, but not sure if
		// % adding an <if> statement would really be useful.
	}

if (t.size() != Dw.size())
	int test=0;



	return Dw;
}
// ---------------------------------------------------------------------------

TSimulationOutput TMainForm::Calc_Model(vector<double>t, vector<double> &parameter_set,
	TPreparedTreatmentConc &ev_ic, int sdit, AnsiString &err)
{
	Application->ProcessMessages();



	/* % Matlab Usage: x_out = calc_model(t,pmat,ev_ic,sdit)
	 %
	 % This function returns the state variables survival probability and scaled
	 % damage for a given time vector, parameter set, and exposure scenario
	 % (time-events matrix). The differential equation for scaled damage is
	 % always solved analytically (done in a separate function
	 % <calc_damage_linear>). Mortality due to chemical stress is calculated in
	 % separate functions, which are also used for the calculation of LCx and
	 % LPx. For IT and for SD with constant exposure, survival probabilities are
	 % calculated analytically. For SD with time-varying exposure, hazard rates
	 % are calculated on a fine time scale and integrated using the trapezium
	 % rule.
	 %
	 % Inputs
	 % <t>      the time vector for which output needs to be returned
	 % <pmat>   the parameter matrix (or vector with parameter values)
	 --> changed to vector<double> parameter_set
	 % <ev_ic>  the exposure scenario for a single treatment as time-events matrix (three columns)
	 % <sdit>   switch for SD (1) or IT (2)
	 %
	 % Outputs
	 % <x_out>  matrix with time in rows, and states in columns (survival probability and scaled damage)
	 %
	 % Author: Tjalling Jager */

	/* %% BLOCK 1. Initial settings
	 % We need to consider the time vector for the calculations. For constant
	 % exposure, we use the one provided in the input <t>. For time-varying
	 % exposure, it depends on the death mechanism. For IT, we'll add the
	 % scenario time points in <ev_ic> to the time vector. For SD, we'll make a
	 % very detailed time vector as we need to numerically integrate hazard
	 % later. */

	TSimulationOutput x_out; // object to return
	vector<double>t_rem = t;
	// remember the original time vector (so we can extract the ones we need as output later)

	int constant; // flag for constant exposure

	constant = 1; // set flag that we have constant exposure
	for (int i = 0; i < ev_ic.Times.size() - 1; i++)
		if (ev_ic.Conc[i] != ev_ic.Conc[i + 1] || ev_ic.Slope[i + 1] != 0)
			// if all concentrations are equal and all slopes are zero
		{
			constant = 0; // set flag for constant exposure to off
			break;
		}
	/* % Then we have a constant exposure concentration, and don't need to do
	 % anything special to the time vector. */

	if (constant == 0)
	{
		/* % For time-varying, we need to make a longer time vector. This could
		 % perhaps be done in <prepare_data>, but since we also use this code
		 % for providing model output for plotting, this is the most transparent
		 % way to implement it (though probably not the fastest). */
		int min_t = t[t.size() - 1] * TIME_POINTS_PER_DAY;
		// minimum length of time vector for GUTS-SD analyses (default each 15 minutes)
		// The same global default is used in <calc_lpx>.
		if (t.size() < min_t) // make sure there are at least <min_t> points
		{ // t = unique([t;(linspace(min(t),max(t),min_t))']); 	// (vector)
			vector<double>t_temp = EquidistantNumbers(t[0], t[t.size() - 1], min_t);
			t.insert(t.end(), t_temp.begin(), t_temp.end());
			sort(t.begin(), t.end());
			t.erase(unique(t.begin(), t.end()), t.end());
		}

	}

	/* %% BLOCK 2. Calculate damage
	 % This part calls the explicit model in <calc_damage_linear> to calculate
	 % the damage over time. However, if we have constant exposure, we can
	 % calculate it directly here, which saves a little bit of time. */

	vector<double>Dw;
	double kd = parameter_set[gutsKD]; // extract the dominant rate constant
	if (constant == 1)
		// for constant concentration, no need to call the function
	{
		Dw.resize(t.size());
		for (int i = 0; i < t.size(); i++)
			Dw[i] = ev_ic.Conc[0] * (1.0 - exp(-kd * t[i]));
		// first-order accumulation (vector)
	}
	else
		Dw = Calc_Damage_Linear(t, kd, ev_ic);
	// call the function to calculate damage <Dw> (vector)

	/* %% BLOCK 3. Calculate survival
	 % We always use the analytical solution for the damage level in
	 % <simple_linear>. Therefore, only the damage level is available at this
	 % point, and the survival probabilities are calculated from it for each
	 % death mechanism separately. Both survival and scaled damage are collected
	 % in a single matrix that will be the output of this function.
	 %
	 % Note: the calls of these functions below are different from each other.
	 % This is a consequence of the fact that they have to serve various (and
	 % different) purposes. For SD, the functions need to be used in conjunction
	 % with <fzero> (root finding) to find LCx or LPx. For IT, these metrics are
	 % calculated directly. */

	vector<double>Sc;
	double hb = parameter_set[gutsHB];
	// extract background hazard rate from <pmat>

	if (t.size() != Dw.size())
		int test=0;

	if (sdit == gutsSD) // stochastic death
	{
		if (constant == 1)
		{
			// for constant exposure, we can use the analytical solution for SD
			Calc_Surv_SD_c(Sc, ev_ic.Conc[0], parameter_set, t, err);
			if (err != "")
				return x_out;
		}
		// only keep survival (<Sc>, vector)
		else // approximate solution with trapezium rule, calculated from <Dw> in a different function
		{
			Calc_Surv_SD_t(Sc, 1.0, parameter_set, t, Dw, err);
			if (err != "")
				return x_out;
		}
		// only keep survival (<Sc>, vector)
	}

	else // individual tolerance, log-logistic distribution is used for GUTS-IT
	{
		/* % Survival under IT at time <t> is determined by the maximum of
		 % <Dw> up to that time point. Therefore, we modify <Dw> to a
		 % <maxDw> vector that does not decrease over time (dead animals
		 % don't become alive). This algorithm is a bit awkward, but pretty
		 % fast in Matlab, especially when there are few episodes with
		 % decrease in <Dw>. */

		vector<double>maxDw = Dw; // copy the vector <Dw> to <maxDw>
		for (int i = 1; i < maxDw.size(); i++)
			if (maxDw[i - 1] > maxDw[i])
				maxDw[i] = maxDw[i - 1];
		/* % Note: <maxDw> is a vector with the maximum damage till that
		 % point, at each time point. */

		Calc_Surv_IT(Sc, maxDw, parameter_set);
		// call function to calculate IT survival from <maxDw>
	}
    if (StopBatchPredictions)
	{
		return x_out;
	}

	vector<double>S = Sc;
	for (int i = 0; i < S.size(); i++)
		S[i] *= exp(-hb * t[i]);
	// multiply survival due to the chemical with survival resulting from background hazard (element-wise: vector)

	x_out.Times = t;
	x_out.SurvivalProb = S;
	x_out.ScaledDamage = Dw;
	// put both state variables in the output matrix as two columns
	// Extract the time points that we need from the total time vector.
	if (t.size() > t_rem.size())
		// only needed if we calculated more points than we were asked to return (not strictly needed, but might be faster)
	{
		vector<double>S_req, Dw_req; // data vectors for requested time points
		int t_rem_it = 0;
		for (int i = 0; i < t.size(); i++)
			if (t[i] == t_rem[t_rem_it])
				// find where the requested time points are in the longer <t> (vector with indices)
			{
				S_req.push_back(S[i]);
				Dw_req.push_back(Dw[i]);
				t_rem_it++;
				if (t_rem_it == t_rem.size())
					break; // break loop if last t_rem element is reached
				i--; // if there are duplicate time points in t_rem, then t[i] has to be checked again
			}

		// only keep the rows we asked for
		if (t_rem.size() == S_req.size() && t_rem.size() == Dw_req.size())
		{
			x_out.Times = t_rem;
			x_out.SurvivalProb = S_req;
			x_out.ScaledDamage = Dw_req;
			// put both state variables in the output matrix as two columns
		}
	}



	return x_out;
}
// ---------------------------------------------------------------------------

bool TMainForm::Startgrid()
{


	/* % Matlab Usage: pmat = startgrid(sdit)
	 %
	 % This function prepares a parameter structure for GUTS analyses, with
	 % min-max ranges, as required for the parameter-space explorer
	 % (<calc_parspace>). As input, it needs to know the setting for SD or IT in
	 % <sdit>.
	 %
	 % Note: at this moment, this function does not allow recalculation of
	 % bounds when the user has fixed some parameters (or set other bounds for
	 % some parameters). Since some bounds depend on the value of other bounds,
	 % this could be reconsidered. In any case, the code requires only little
	 % modification to allow a <pmat> to be entered by the user.
	 %
	 % Inputs
	 % <sdit>  switch for SD or IT calculation (they have different ranges)
	 %
	 % Outputs
	 % <pmat>  matrix with parameter info (best, fit/fix, min, max, log/norm)
	 %
	 % * Author     : Tjalling Jager */

	/* %% BLOCK 1. Preliminary things
	 % First some preliminary checks. */

	if (ConcUnit == "")
		// if the concentration unit is not defined (this should not happen anymore with the standard file format)
			ConcUnit = "[C]"; // set a default

	// Defining the basic structure of the <pmat> matrix.
	for (int p = 0; p < gutsPARAMETER_NO; p++)
	{
		// set value:
		PMatIT.Value[p] = 0.0;
		PMatSD.Value[p] = 0.0;
		// set fit/not fit:
		PMatSD.Fit[p] = gutsFIT_PARAM;
		PMatIT.Fit[p] = gutsFIT_PARAM;
		// set minimum:
		PMatSD.Min[p] = 0.0;
		PMatIT.Min[p] = 0.0;
		// set maximum:
		PMatSD.Max[p] = 0.0;
		PMatIT.Max[p] = 0.0;
		// set norm/log scale:
		if (p == gutsMW || p == gutsHB)
		{
			PMatSD.Scale[p] = gutsNORM_SCALE_PARAM;
			PMatIT.Scale[p] = gutsNORM_SCALE_PARAM;
		}
		else
		{
			PMatSD.Scale[p] = gutsLOG_SCALE_PARAM;
			PMatIT.Scale[p] = gutsLOG_SCALE_PARAM;
		}
	}
	// Make sure that right parameters are fitted for each special case:
	// for SD, never fit the threshold spread, set to 1, normal scale
	PMatSD.Value[gutsFS] = 1.0;
	PMatSD.Fit[gutsFS] = gutsNOT_FIT_PARAM;
	PMatSD.Scale[gutsFS] = gutsNORM_SCALE_PARAM;
	// for IT, never fit the killing rate, set to infinity, normal scale
	PMatIT.Value[gutsBW] = -1; // place holder for 'INF'
	PMatIT.Fit[gutsBW] = gutsNOT_FIT_PARAM;
	PMatIT.Scale[gutsBW] = gutsNORM_SCALE_PARAM;

	if (PrefitHBToControl)
		// if we want to fit hb to the control value (and it is available)
	{
		PMatSD.Value[gutsHB] = CalibData_HB.BackgroundHazard;
		// use hb from calculations based on prepare data function
		PMatIT.Value[gutsHB] = CalibData_HB.BackgroundHazard;
		PMatSD.Fit[gutsHB] = gutsNOT_FIT_PARAM; // never fit hb
		PMatIT.Fit[gutsHB] = gutsNOT_FIT_PARAM;
		PMatSD.Scale[gutsHB] = gutsNORM_SCALE_PARAM; // set to normal scale
		PMatIT.Scale[gutsHB] = gutsNORM_SCALE_PARAM;
	}

	/* % For parameters that are fixed, copy the entered value in the first column
	 % to both ranges. This may help to obtain smaller search ranges for some of
	 % the other parameters (when the range is calculated from another range),
	 % and it looks better when printing results on screen. */
	for (int p = 0; p < gutsPARAMETER_NO; p++)
	{
		if (PMatSD.Fit[p] == gutsNOT_FIT_PARAM)
		{
			PMatSD.Min[p] = PMatSD.Value[p];
			PMatSD.Max[p] = PMatSD.Value[p];
		}
		if (PMatIT.Fit[p] == gutsNOT_FIT_PARAM)
		{
			PMatIT.Min[p] = PMatIT.Value[p];
			PMatIT.Max[p] = PMatIT.Value[p];
		}
	}

	/* %% BLOCK 2. Find min-max time and exposure concentration across all data sets.
	 % Make use of the info already stored for each data set. This could also be
	 % done in <prepare_data> ... however, since the rules for the starting
	 % ranges may be changed in the future, it is probably safer to keep them
	 % here. */

	double t_max = HIGH_NEG_DOUBLE; // maximum time
	double t_min = HIGH_POS_DOUBLE; // minimum *non zero* time
	double c_max = HIGH_NEG_DOUBLE;
	// look for maximum peak concentration across all data sets
	double c_min = HIGH_POS_DOUBLE;
	// look for the minimum *non-zero* TWA across all data sets

	for (int d = 0; d < CalibData_All.Survival.size(); d++) // run through all data sets for calibration
	{
		// look for the minimum *non-zero* TWA across all data sets
		for (int g = 0; g < CalibData_All.TWAConcentration[d].size(); g++)
			if (CalibData_All.TWAConcentration[d][g] < c_min && CalibData_All.TWAConcentration[d][g] > 0)
				c_min = CalibData_All.TWAConcentration[d][g];
		// look for maximum peak concentration across all data sets
		for (int g = 0; g < CalibData_All.MaxConcentration[d].size(); g++)
			if (CalibData_All.MaxConcentration[d][g] > c_max)
				c_max = CalibData_All.MaxConcentration[d][g];

		for (int g = 0; g < CalibData_All.Survival[d].size(); g++)
		{
			// look for maximum time point across all data sets
			if (CalibData_All.Survival[d][g].Times[CalibData_All.Survival[d][g].Times.size() - 1] > t_max)
				t_max = CalibData_All.Survival[d][g].Times[CalibData_All.Survival[d][g].Times.size() - 1];
			// look for minimum *non zero* time point across all data sets
			if (CalibData_All.Survival[d][g].Times[1] < t_min)
				t_min = CalibData_All.Survival[d][g].Times[1];
			/* % Since the time vector is unique, and the first entry zero, the second
			 % element is the time for the first observation on survival. */
		}
	}

	if (t_max == 0.0 || t_min == 0.0 || c_max == 0.0)
	{
		ShowMessage("Error while deriving parameter ranges. Please provide reasonable input data (max. conc. > 0, second and last observation time point > 0).");
		return false;
    }

	/* %% BLOCK 3. Find relevant min-max ranges based on the data set.
	 % Rules of thumb are used to restrict parameter space, based on the data
	 % set. */

	// look for hb
	if (PMatSD.Fit[gutsHB] == gutsFIT_PARAM)
		// only if <hb> is fitted (otherwise, a value is provided in the input)
	{
		// simply use a fixed upper boundary for <hb> for now
		PMatSD.Min[gutsHB] = 0.000001;
		PMatSD.Max[gutsHB] = 0.07;
		/* % Note: the minimum value is also set in <prepare_data> for fitting
		 % <hb> on the controls. If we change the value here, it is best also
		 % changed there! */
	}
	else
	{
		PMatSD.Min[gutsHB] = CalibData_All.BackgroundHazard;
		PMatSD.Max[gutsHB] = CalibData_All.BackgroundHazard;
	}
	if (PMatIT.Fit[gutsHB] == gutsFIT_PARAM)
		// only if <hb> is fitted (otherwise, a value is provided in the input)
	{
		// simply use a fixed upper boundary for <hb> for now
		PMatIT.Min[gutsHB] = 0.000001;
		PMatIT.Max[gutsHB] = 0.07;
		/* % Note: the minimum value is also set in <prepare_data> for fitting
		 % <hb> on the controls. If we change the value here, it is best also
		 % changed there! */
	}
	else
	{
		PMatIT.Min[gutsHB] = CalibData_All.BackgroundHazard;
		PMatIT.Max[gutsHB] = CalibData_All.BackgroundHazard;
	}

	// look for kd
	if (PMatSD.Fit[gutsKD] == gutsFIT_PARAM) // only if <kd> is fitted
	{
		/* % In principle, fixed bounds for <kd> will be used. They are set at 95%
		 % of steady state within 0.5 hour (resolution of FOCUS is 1 hour), and
		 % 95% of steady state achieved in 5 years. However, if people do tests
		 % with a first observation within hours, or run extremely long tests,
		 % we need to extend them. The extension is based on the time vector:
		 % only 5% of steady state achieved at the end of the test, and 99% of
		 % steady state achieved within 1/10 of the first observation. In
		 % practice, these seem to be the identifiability limits. */

		PMatSD.Min[gutsKD] = Min(log(20.0) / (5.0 * 365.0), -log(1.0 - 0.05) / t_max);
		PMatSD.Max[gutsKD] = Max(log(20.0) / (0.5 / 24.0), -log(1.0 - 0.99) / (0.1 * t_min));
	}
	if (PMatIT.Fit[gutsKD] == gutsFIT_PARAM) // only if <kd> is fitted
	{
		PMatIT.Min[gutsKD] = Min(log(20.0) / (5.0 * 365.0), -log(1.0 - 0.05) / t_max);
		PMatIT.Max[gutsKD] = Max(log(20.0) / (0.5 / 24.0), -log(1.0 - 0.99) / (0.1 * t_min));
	}

	// look for mw:
	if (PMatSD.Fit[gutsMW] == gutsFIT_PARAM) // only if <mw> is fitted
	{
		// Minimum is body residue that lowest conc. will achieve in 4 hours at lowest value of <kd>.
		PMatSD.Min[gutsMW] = c_min * (1.0 - exp(-PMatSD.Min[gutsKD] * (4.0 / 24.0)));
		PMatSD.Max[gutsMW] = 0.99 * c_max;
		// for SD, max as 99% of maximum peak concentration
	}
	if (PMatIT.Fit[gutsMW] == gutsFIT_PARAM) // only if <mw> is fitted
	{
		// Minimum is body residue that lowest conc. will achieve in 4 hours at lowest value of <kd>.
		PMatIT.Min[gutsMW] = c_min * (1.0 - exp(-PMatIT.Min[gutsKD] * (4.0 / 24.0)));
		PMatIT.Max[gutsMW] = 2 * c_max;
		// for IT, max as two times of maximum peak concentration
	}

	// look for bw
	if (PMatSD.Fit[gutsBW] == gutsFIT_PARAM) // only if <bw> is fitted
	{
		/* % Min is 10% effect at end of test when fast kinetics and threshold
		 % zero. Max is that what you gain in damage in one hour, at lowest
		 % exposure, at minimum <kd>, is enough to kill you (95% sure) in 1 hour. */
		PMatSD.Min[gutsBW] = -log(0.9) / (c_max * t_max);
		PMatSD.Max[gutsBW] = (pow(24.0, 2.0) * 0.95) / (PMatSD.Min[gutsKD] * c_max * exp(-PMatSD.Min[gutsKD] * t_max * 0.5));
	}

	// look for Fs
	if (PMatIT.Fit[gutsFS] == gutsFIT_PARAM) // only if <Fs> is fitted
	{
		PMatIT.Min[gutsFS] = 1.05; // this range seems good enough  // changed from 1.0 to 1.05
		PMatIT.Max[gutsFS] = 20.0;
	}

	/* %% BLOCK 4. Finish with best value
	 % Value is not important: it is overwritten by <calc_parspace> anyway.
	 % However, it needs to be within the bounds to avoid errors. */

	for (int p = 0; p < gutsPARAMETER_NO; p++)
	{
		PMatSD.Value[p] = PMatSD.Max[p];
		// nothing fancy, just use maximum for the best-value column for now
		PMatIT.Value[p] = PMatIT.Max[p];
	}

	// limit of 4 regarding the significant digits
	AnsiString temp_as;
	for (int p = 0; p < gutsPARAMETER_NO; p++)
	{
		// SD
		temp_as = MyFloatToStr(PMatSD.Value[p], 4);
		PMatSD.Value[p] = temp_as.ToDouble();
		temp_as = MyFloatToStr(PMatSD.Min[p], 4);
		PMatSD.Min[p] = temp_as.ToDouble();
		temp_as = MyFloatToStr(PMatSD.Max[p], 4);
		PMatSD.Max[p] = temp_as.ToDouble();
		// IT
		temp_as = MyFloatToStr(PMatIT.Value[p], 4);
		PMatIT.Value[p] = temp_as.ToDouble();
		temp_as = MyFloatToStr(PMatIT.Min[p], 4);
		PMatIT.Min[p] = temp_as.ToDouble();
		temp_as = MyFloatToStr(PMatIT.Max[p], 4);
		PMatIT.Max[p] = temp_as.ToDouble();
	}



	return true;
}
// ---------------------------------------------------------------------------

bool TMainForm::DeriveParamRangesFromData()
{
	// check the data in the grids
	if (!CheckCalibrationDataSets())
		return false;

	// CHECK and GET the data from the grids
	CheckAndGetCalibrationData(CalibrationData);

	// prepare the calibration data sets
	Prepare_Data(CalibData_All, CalibData_HB, CalibrationData,
			NumberOfCalibrationDataSets, PrefitHBToControl);

	// This function prepares a parameter structure for GUTS analyses, with min-max ranges, as required for the parameter-space explorer (<calc_parspace>):
	if (!Startgrid())
		return false;

    ParameterRangesManipulated = false;

	return true;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::DeriveSDParamRangesFromDataButtonClick(TObject *Sender)
{
	if (!ResetCalibrationSettingInGUI())
		return;
	/*
	Screen->Cursor = crHourGlass;

	// get parameter ranges
	DeriveParamRangesFromData();
	// display the matrix in the GUI
	bool change_value = false;
	bool change_range = true;
	DisplayParameterMatrixInGUI(PMatSD, gutsSD, change_value, change_range);
	DisplayParameterMatrixInGUI(PMatIT, gutsIT, change_value, change_range);

	ParameterRangesManipulated = false;

	Screen->Cursor = crDefault;*/

	return;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::FitHBToControlCheckBoxClick(TObject *Sender)
{
	PrefitHBToControl = FitHBToControlCheckBox->Checked;

	if (FitHBToControlCheckBox->Checked)
	{
		GUTSSDParamFitChecks[gutsHB]->Checked = false;
		GUTSITParamFitChecks[gutsHB]->Checked = false;
		GUTSSDParamValueEdits[gutsHB]->Enabled = false;
		GUTSITParamValueEdits[gutsHB]->Enabled = false;
	}
	else
	{
		GUTSSDParamFitChecks[gutsHB]->Checked = true;
		GUTSITParamFitChecks[gutsHB]->Checked = true;
		GUTSSDParamValueEdits[gutsHB]->Enabled = true;
		GUTSITParamValueEdits[gutsHB]->Enabled = true;
	}

	if (!ResetCalibrationSettingInGUI())
		return;

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::DisplayParameterMatrixInGUI(TParameterMatrix pmat, int survival_model,
	bool change_value, bool change_range)
{
	if (survival_model == gutsSD)
		for (int p = 0; p < gutsPARAMETER_NO; p++)
		{
			if (change_value)
				GUTSSDParamValueEdits[p]->Text = MyFloatToStr(pmat.Value[p], 4);
			if (change_range)
			{
				GUTSSDParamMinEdits[p]->Text = MyFloatToStr(pmat.Min[p], 4);
				GUTSSDParamMaxEdits[p]->Text = MyFloatToStr(pmat.Max[p], 4);
			}
		}

	if (survival_model == gutsIT)
	{
		for (int p = 0; p < gutsPARAMETER_NO; p++)
		{
			if (change_value)
			{
				if (p == gutsBW)    // special case to display Inf in the GUI (for IT, killing rate bw=Inf)
					GUTSITParamValueEdits[p]->Text = "Inf";
				else
					GUTSITParamValueEdits[p]->Text = MyFloatToStr(pmat.Value[p], 4);
			}
			if (change_range)
			{
				if (p == gutsBW)    // special case to display Inf in the GUI (for IT, killing rate bw=Inf)
				{
					GUTSITParamMinEdits[p]->Text = "Inf";
					GUTSITParamMaxEdits[p]->Text = "Inf";
				}
				else
				{
					GUTSITParamMinEdits[p]->Text = MyFloatToStr(pmat.Min[p], 4);
					GUTSITParamMaxEdits[p]->Text = MyFloatToStr(pmat.Max[p], 4);
				}
			}
		}
	}

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::ShowCellContent(TStringGrid *grid, int x_pos, int y_pos)
{
	int row, col;

	grid->ShowHint = true; // enable hints
	grid->MouseToCell(x_pos, y_pos, col, row); // get cell from coordinates
	if (col > -1 && row > -1)
		grid->Hint = grid->Cells[col][row];
	if (row != LastCellY || col != LastCellX) // look for cell change
	{
		Application->CancelHint(); // cancel hint
		LastCellX = col; // update current cell
		LastCellY = row;
	}

	return;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::CalibConcStringGrid1MouseMove(TObject *Sender, TShiftState Shift, int X, int Y)
{
	TStringGrid *grid = (TStringGrid*)Sender;

	ShowCellContent(grid, X, Y);

	return;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::PredDataOpenSpeedClick(TObject *Sender)
{
	if (InputFileOpenDialog->Execute())
	{
		WritePredictionsDataFileToGrid(AnsiString(InputFileOpenDialog->FileName).c_str(), PredictionsConcStringGrid);
		PredictionsDataFileName = AnsiString(InputFileOpenDialog->FileName);
		ClearPredictionsResults();
	}

	return;
}
// ---------------------------------------------------------------------------

bool TMainForm::WritePredictionsDataFileToGrid(string filename, TStringGrid *pred_grid)
	// open prediction data file and load it into a string grid
	/*
	 Data format as received from Tjalling Jager:
	 0.000	1.379E-03
	 0.042	8.058E-03
	 0.083	1.496E-02
	 0.125	2.083E-02
	 0.167	2.585E-02
	 ...  	...
	 */ {
	// clear grid
	ClearGrid(pred_grid);

	Screen->Cursor = crHourGlass;
	ifstream ifile(filename.c_str());
	string s;
	vector<string>sv;
	int grid_row = pred_grid->FixedRows;

	while (!ifile.eof())
	{
		GetNoCommentLine(ifile, s);
		sv = SplitString(s, "\t");
		if (grid_row >= pred_grid->RowCount)
			pred_grid->RowCount++;
		for (int c = 0; c < sv.size(); c++)
			pred_grid->Cells[c][grid_row] = sv[c].c_str();
		grid_row++;
	}

	Screen->Cursor = crDefault;

	return true;
}
// ---------------------------------------------------------------------------

vector<TParameterSpace> TMainForm::ParameterSpaceSubsetForErrorPropagation(const vector<TParameterSpace> &sample)
{
	vector<TParameterSpace> sub_sample;
	sub_sample.resize(0);

	if (sample.size() < 1)
		return sub_sample;

	double mll = sample[0].Likelihood;

	int ind_prop1 = sample.size() - 1;
	bool ind_prob1_found = false;
	int ind_prop2 = sample.size() - 1;

	for (int k = sample.size() - 1; k >= 0; k--)
		if (sample[k].Likelihood < mll + 0.5 * gutsCRIT_PROP[0])
		{
			ind_prop1 = k;
			ind_prob1_found = true;
			break;
		}
	for (int k = sample.size() - 1; k >= 0; k--)
		if (sample[k].Likelihood < mll + 0.5 * gutsCRIT_PROP[1])
		{
			ind_prop2 = k;
			break;
		}

	if (!ind_prob1_found)
		ind_prop1 = 0;

	for (int i=ind_prop1; i<=ind_prop2; i++)
		sub_sample.push_back(sample[i]);


	return sub_sample;
}
// ---------------------------------------------------------------------------

void TMainForm::Calc_Conf(TSimulationOutput &x_out, vector<double> &t,
	TPreparedTreatmentConc &ev_ic, int sdit, double &hb_fix, int flag_conf, AnsiString &err)
{
	// Usage: [model_p,model_dam] = calc_conf(t,ev_ic,sdit,hb_fix,flag_conf)
	//
	// This function creates confidence intervals on model curves from the
	// parameter-space sample that was saved by <calc_parspace>. This script
	// calculates the best model curve and the bounds from the saved set. There
	// is the option to fix the background hazard to a specific value. This is
	// helpful when validation uses a fixed <hb> and calibration did not. Also,
	// this is helpful when plotting the results for an LPx analysis: to plot
	// survival vs. time for a specific multiplication factor. This implies that
	// we have to set background hazard to zero.
	//
	// Inputs
	// <t>          time vector for calculating model output
	// <ev_ic>      an exposure-events matrix for a single treatment
	// <sdit>       switch for SD (1) or IT (2)
	// <hb_fix>     fixed value for the background hazard rate (-1 is not fixed)
	// <flag_conf>  1) calculate CIs, 0) do not calculate CIs
	//
	// Outputs; two matrices with three columns each (best, minimum, maximum)
	// <model_p>    the best curve for survival probability, and CI
	// replaced by x_out.SurvivalProbLowerCI / ...UpperCI
	// <model_dam>  the best curve for damage, and CI
	// replaced by x_out.ScaledDamageLowerCI / ...UpperCI
	//
	// Author     : Tjalling Jager

	//// BLOCK 1. Initial things

	TParameterMatrix pmat;
	vector<double> parameter_set;
	vector<TParameterSpace> sample;

	// Load file as saved by <calc_parspace> and prepare sample.
	if (sdit == gutsSDIT_BOTH)
	{
		Calc_Conf(x_out, t, ev_ic, gutsSD, hb_fix, flag_conf, err);
		if (err != "")
			return;
		Calc_Conf(x_out, t, ev_ic, gutsIT, hb_fix, flag_conf, err);
		if (err != "")
			return;
		return;
	}
	if (sdit == gutsSD)
	{
		pmat = PMatSD;
		sample = CalibrationParameterSpaceSD;
	}
	else
	{
		pmat = PMatIT;
		sample = CalibrationParameterSpaceIT;
	}

	sample = ParameterSpaceSubsetForErrorPropagation(sample);

	// Apply fixed background hazard rate when/where needed.
	if (hb_fix != -1)
		// then we are requested to include a fixed hazard rate (even when <hb> was fitted in calibration)
	{
		pmat.Value[gutsHB] = hb_fix; // set background hazard to <hb_fix> and
		pmat.Scale[gutsHB] = gutsNORM_SCALE_PARAM;
		// make sure it is normal scale!
		for (int s = 0; s < sample.size(); s++)
			sample[s].Parameter[gutsHB] = hb_fix;
		// also make it <hb_fix> in all sets of the sample!
	}
	parameter_set = pmat.Value;

	//// BLOCK 2. Calculate model curves for each set in the sample.

	// BLOCK 2.1. Start by running the best fitting set (in <pmat>) to produce a
	// best-fitting curve.

	// Initialise the matrices that will collect the model output (with CIs).

	x_out = Calc_Model(t, parameter_set, ev_ic, sdit, err);
	if (err != "")
		return;
	if (StopBatchPredictions)
	{
		return;
	}
	// use <calc_model> to provide the output for this scenario (2-column matrix)

	// BLOCK 2.2. Prepare for running through the sample.

	// ind_log = pmat(:,5)==0;   // vector with logical indices to parameters on log scale

	vector<double>survival_prob_rem = x_out.SurvivalProb;
	vector<double>survival_prob_lowerCI_rem = x_out.SurvivalProb;
	vector<double>survival_prob_upperCI_rem = x_out.SurvivalProb;
	vector<double>scaled_damage_rem = x_out.ScaledDamage;
	vector<double>scaled_damage_lowerCI_rem = x_out.ScaledDamage;
	vector<double>scaled_damage_upperCI_rem = x_out.ScaledDamage;
	// Note: this returns the CIs as NaN, so they are ignored when plotting
	// do it in another way in C++
	x_out.SurvivalProbLowerCI = x_out.SurvivalProb;
	x_out.SurvivalProbUpperCI = x_out.SurvivalProb;
	x_out.ScaledDamageLowerCI = x_out.ScaledDamage;
	x_out.ScaledDamageUpperCI = x_out.ScaledDamage;
	if (flag_conf == 0)
		// then there is no need to return CIs, just the best model line
			return;

	// BLOCK 2.3. Run through the sample to calculate CIs on curves.
	for (int s = 0; s < sample.size(); s++) // run through all sets in the sample
	{
		parameter_set = sample[s].Parameter;
		// Put parameters that need to be on log scale back on normal scale (as
		// <calc_model> requires normal scale).
		for (int p = 0; p < gutsPARAMETER_NO; p++)
			if (pmat.Scale[p] == gutsLOG_SCALE_PARAM)
			{
				if (parameter_set[p] < log10(HIGH_POS_DOUBLE))
					parameter_set[p] = pow(10.0, parameter_set[p]);
				else
					parameter_set[p] = HIGH_POS_DOUBLE;
			}

		// Note: <pmat> is still on normal scale here, but the sample contains
		// the value on a log scale, if a parameter is fitted on log scale.
		// <calc_model> needs normal scale <pmat> (also for the fixed parameters).

		x_out = Calc_Model(t, parameter_set, ev_ic, sdit, err);
		if (err != "")
			return;
        if (StopBatchPredictions)
		{
			return;
		}
		// use <calc_model> to provide the model output for this scenario

		// I don't save all individual curves, but just update min-max for every
		// set. Note that this takes the minimum across two columns from a matrix,
		// for each row.
		for (int i = 0; i < x_out.SurvivalProb.size(); i++)
		{
			survival_prob_lowerCI_rem[i] = Min(survival_prob_lowerCI_rem[i], x_out.SurvivalProb[i]);
			// smallest value of current lowest and new model line
			survival_prob_upperCI_rem[i] = Max(survival_prob_upperCI_rem[i], x_out.SurvivalProb[i]);
			// largest value of current lowest and new model line
			scaled_damage_lowerCI_rem[i] = Min(scaled_damage_lowerCI_rem[i], x_out.ScaledDamage[i]);
			// smallest value of current lowest and new model line
			scaled_damage_upperCI_rem[i] = Max(scaled_damage_upperCI_rem[i], x_out.ScaledDamage[i]);
			// largest value of current lowest and new model line
		}
	}

	x_out.SurvivalProb = survival_prob_rem;
	x_out.SurvivalProbLowerCI = survival_prob_lowerCI_rem;
	x_out.SurvivalProbUpperCI = survival_prob_upperCI_rem;
	x_out.ScaledDamage = scaled_damage_rem;
	x_out.ScaledDamageLowerCI = scaled_damage_lowerCI_rem;
	x_out.ScaledDamageUpperCI = scaled_damage_upperCI_rem;

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::Calc_LCx(vector<vector<double> > &LCx, vector<vector<double> > &LC_low, vector<vector<double> > &LC_high,
	double t_end, int sdit, AnsiString &err)
{
	// Matlab Usage: [LCx,LClo,LChi] = calc_lcx(Tend,sdit)
	//
	// Function to calculate LCx,t values, with confidence intervals. For IT,
	// this function uses the complete analytical solution for the LCx,t
	// (provided in the function <calc_surv_it>). For SD, it uses the analytical
	// solution for survival (in <calc_surv_sd_c>), but still needs <fzero> to
	// find the LCx. If we ever need to implement the full GUTS model (with
	// separate TK and damage) some shortcuts need to be reconsidered!
	//
	// The output of this function is used for testing purposes only: matrices
	// for LCx,t and the corresponding lower and higher end of the CI. No
	// outputs would be needed as this function prints results to screen and to
	// the diary.
	//
	// Inputs
	// <Tend>   the maximum time point for the LCx-time plot
	// <sdit>   switch for SD (1) or IT (2)
	//
	// Outputs
	// <LCx>    matrix with LCx,t values (time in rows, increasing, x in columns, decreasing)
	// <LClo>   lower edge of the 95% CI; same format as <LCx>
	// <LChi>   upper edge of the 95% CI; same format as <LCx>
	//
	// Author     : Tjalling Jager

	if (sdit == gutsSDIT_BOTH)
	{
		Calc_LCx(LCx, LC_low, LC_high, t_end, gutsSD, err);
		if (err != "")
			return;
		Calc_LCx(LCx, LC_low, LC_high, t_end, gutsIT, err);
        if (err != "")
			return;
		return;
	}

	//// BLOCK 1. Initial things.

	string savestr;
	TParameterMatrix pmat;
	vector<double>parameter_set;
	vector<TParameterSpace>sample;
	vector<double>Sc;
	vector<double>maxDw;
	double LPx;
	vector<double>LCinit;
	LCinit.resize(2);
	vector<double>t_lcx_tmp;
	t_lcx_tmp.resize(1);

	// Create text string for plotting titles and saving plots.
	if (sdit == gutsSD)
		savestr = "_LCxt_SD";
	else
		savestr = "_LCxt_IT";

	// These checks are no longer needed as we now have fixed time points and
	// fixed effect fractions. However, to be on the safe side ... Note: time
	// needs to increase and effect fraction need to decrease to reduce the
	// range that <fzero> has to search for SD (as LCx,t has to decrease with
	// increasing time and with dereasing x).

	//// BLOCK 2. Load calibration file as saved by <calc_parspace>.
	if (sdit == gutsSD)
	{
		pmat = PMatSD;
		sample = CalibrationParameterSpaceSD;
	}
	else
	{
		pmat = PMatIT;
		sample = CalibrationParameterSpaceIT;
	}
	parameter_set = pmat.Value;

	sample = ParameterSpaceSubsetForErrorPropagation(sample);

	//// BLOCK 3. Calculate LCx,t
	// Several model functions will be used to (help) find the LCx,t. For IT,
	// LCx,t can be calculated analytically, but for SD, use of <fzero> is
	// needed. In the latter case, the analytical solution for survival is used,
	// which is possible as the LCx is per definition for constant exposure.
	// This implies that <calc_model> is NOT used for these calculations.
	// Background hazard is ignored as LCx,t is relative to the control.

	// Initialise matrices to collect the LCx and CIs over time for the different effect levels.
	LCx.resize(predT_LCX_NO);
	LC_low.resize(predT_LCX_NO);
	LC_high.resize(predT_LCX_NO);
	for (int i = 0; i < LCx.size(); i++)
	{
		LCx[i].resize(predF_LCX_NO);
		LC_low[i].resize(predF_LCX_NO);
		LC_high[i].resize(predF_LCX_NO);
		for (int j=0; j<LCx[i].size(); j++)
		{
			LC_low[i][j] = HIGH_POS_DOUBLE;
			LC_high[i][j] = HIGH_NEG_DOUBLE;
        }
	}

	maxDw.resize(1);
	maxDw[0] = 1.0;
	double a, b;
	if (sdit == gutsIT)
		// for IT, we can use a direct calculation in <calc_surv_it>
		for (int t = 0; t < LCx.size(); t++) // run through time points for the LCx
			for (int e = 0; e < LCx[t].size(); e++) // run through effect levels requested
					Calc_Surv_IT(Sc, LCx[t][e], LPx, maxDw, parameter_set, (double)predT_LCX[t], (double)predF_LCX[e]);
	// calculate LCx
	else // for SD, some more work is needed as we need to use <fzero>
	{
		/*LCinit[0] = pmat.Value[gutsMW];
		// initial guess for LCx,t ... (just take <mw>: single value)
		LCinit[1] = pmat.Max[gutsMW] * 100;*/

		// NEW: make sure that LCinit is always a range
		LCinit[0] = parameter_set[gutsMW] / 10.0;  // as search range for LCx,t start with <mw>/10 and <mw>
		LCinit[1] = parameter_set[gutsMW];
		// Using <mw> is fine for lower end of range but not for upper, so find
		// a better value for the upper range by trial and error. I do this
		// by shifting this initial range up until it covers the sign change
		// of <crit>.
		double crit = 1; // first one will be positive
		vector<double> dummy_surv, t_temp;
		t_temp.resize(1);
		t_temp[0] = predT_LCX[0];
		while (crit > 0)
		{
			LCinit[0] *= 10.0; // shift lower and upper range by factor of 10
			LCinit[1] *= 10.0;

			Calc_Surv_SD_c(crit, dummy_surv, LCinit[1], parameter_set, t_temp, predF_LCX[0], err);   // calculate criterion from upper range
			if (err != "")
				return;

			//crit = calc_surv_sd_c(LCinit(2),pmat,T_LCx(1),F_LCx(1));
		}
		// this makes sure that there is a range in which there IS an LCx,t for the first time point and highest effect level
        // END of NEW

		for (int e = 0; e < predF_LCX_NO; e++) // run through effect levels requested
		{
			for (int t = 0; t < predT_LCX_NO; t++) // run through time points for the LCx
			{
				t_lcx_tmp[0] = predT_LCX[t];
				Setup_Calc_Surv_SD(parameter_set, t_lcx_tmp, predF_LCX[e]);
				ERR = "";
				LCx[t][e] = brent::zero(LCinit[0], LCinit[1], calibROOT_FINDING_TOL, Calc_Surv_SD_External);
				if (ERR != "")
					return;
				// LCx[t][e] = fzero(@calc_surv_sd_c,LCinit,[],pmat,T_LCx(i_t),F_LCx(i_e)); // find the LCx,t
				// Note: the empty matrix is the place for optional options for <fzero>.
				LCinit[0] = parameter_set[gutsMW];
				// create new starting *range* (vector with 2 values)
				LCinit[1] = 1.01 * LCx[t][e];
				// Make use of the fact that LCx always decreases in time, and cannot be lower than <mw>!
			}
			LCinit[0] = parameter_set[gutsMW];
			LCinit[1] = 1.01 * LCx[0][e];
			// Before we go to the next effect level, set the upper range for
			// LCx to the first value in this run: since we arranged <F_LCx>
			// from high to low x, the next first value will be lower than this
			// one!
		}
	}

	//// BLOCK 4. Make a plot of LCx versus time
	// Start making a plot, even though we don't have the CIs yet. This gives
	// the user something to look at while the calculations proceed.

	//// BLOCK 5. Calculate CIs on the LCx,t.
	// Note that I do not collect all LCx,t values for all parameter sets in the
	// sample. Instead, I only update the min-max for each LCx,t value.

	// BLOCK 5.1. Initial things.
	for (int n = 0; n < sample.size(); n++) // run through all sets in the sample
	{
		// BLOCK 5.2. Replace parameters in <pmat> with the values in the next
		// set of the sample.
		parameter_set = sample[n].Parameter;
		// replace values in <pmat> with the next set in the sample
		// Put parameters that need to be on log scale back on normal scale (as
		// the SD/IT functions require normal scale).
		for (int p = 0; p < gutsPARAMETER_NO; p++)
			if (pmat.Scale[p] == gutsLOG_SCALE_PARAM)
			{
				if (parameter_set[p] < log10(HIGH_POS_DOUBLE))
					parameter_set[p] = pow(10.0, parameter_set[p]);
				else
					parameter_set[p] = HIGH_POS_DOUBLE;
			}
		// Note: <pmat> is on normal scale here, but the sample contains the
		// value on a log scale, if a parameter is fitted on log scale.

		// BLOCK 5.3. For IT ...
		double LCtmp;
		if (sdit == gutsIT) // for IT, we can use a direct calculation
			for (int e = 0; e < predF_LCX_NO; e++) // run through effect levels requested
				for (int t = 0; t < predT_LCX_NO; t++) // run through times requested
				{
					Calc_Surv_IT(Sc, LCtmp, LPx, maxDw, parameter_set, (double)predT_LCX[t], (double)predF_LCX[e]);
					// calculate LCx
					LC_low[t][e] = Min(LC_low[t][e], LCtmp);
					// keep smallest value of current lowest and new LCx (matrix)
					LC_high[t][e] = Max(LC_high[t][e], LCtmp);
					// keep largest value of current largest and new LCx (matrix)
				}
		// BLOCK 5.4. For SD ...
		else // for SD, some more work is needed; need to use <fzero>
		{
			/*LCinit[0] = pmat.Value[gutsMW];
			// LCx[0][0]; // just an initial guess for the first LCx,t ... (it should be close to the first one for the best set)
			LCinit[1] = pmat.Max[gutsMW] * 100; // LCx[0][0];*/

			// NEW: make sure that LCinit is always a range
			LCinit[0] = parameter_set[gutsMW] / 10.0;  // as search range for LCx,t start with <mw>/10 and <mw>
			LCinit[1] = parameter_set[gutsMW];
			// Using <mw> is fine for lower end of range but not for upper, so find
			// a better value for the upper range by trial and error. I do this
			// by shifting this initial range up until it covers the sign change
			// of <crit>.
			double crit = 1; // first one will be positive
			vector<double> dummy_surv, t_temp;
			t_temp.resize(1);
			t_temp[0] = predT_LCX[0];
			while (crit > 0)
			{
				LCinit[0] *= 10.0; // shift lower and upper range by factor of 10
				LCinit[1] *= 10.0;

				Calc_Surv_SD_c(crit, dummy_surv, LCinit[1], parameter_set, t_temp, predF_LCX[0], err);   // calculate criterion from upper range
				if (err != "")
					return;

				//crit = calc_surv_sd_c(LCinit(2),pmat,T_LCx(1),F_LCx(1));
			}
			// this makes sure that there is a range in which there IS an LCx,t for the first time point and highest effect level

			vector<double>LCinit_new;
			LCinit_new.resize(2);
			for (int e = 0; e < predF_LCX_NO; e++) // run through effect levels requested
			{
				for (int t = 0; t < predT_LCX_NO; t++) // run through time points for the LCx
				{
					t_lcx_tmp[0] = predT_LCX[t];
					Setup_Calc_Surv_SD(parameter_set, t_lcx_tmp, predF_LCX[e]);
					ERR = "";
					LCtmp = brent::zero(LCinit[0], LCinit[1], calibROOT_FINDING_TOL, Calc_Surv_SD_External);
					if (ERR != "")
						return;
					// LCtmp  = fzero(@calc_surv_sd_c,LCinit,[],pmat,T_LCx(i_t),F_LCx(i_e)); // find the LCx,t with <fzero>
					LCinit[0] = parameter_set[gutsMW];
					// update <LCinit>, and now make it a range for <fzero> to search
					LCinit[1] = 1.01 * LCtmp;
					// Make use of the fact that LC50s always decrease in time, and cannot be lower than <mw>!
					LC_low[t][e] = Min(LC_low[t][e], LCtmp);
					// smallest value of current lowest and new LCx
					LC_high[t][e] = Max(LC_high[t][e], LCtmp);
					// largest value of current largest and new LCx

					if (t == 0) // for the first time point ...
							LCinit_new = LCinit;
					// collect the updated search range as start value for next effect level
				}
				LCinit = LCinit_new;
				// For the next effect level, set the upper range for LCx to the
				// first value in this run: since we arranged <F_LCx> from high
				// to low x, the next first value will be lower than this one!
			}
		}
	}

	//// BLOCK 6. Put results on screen.

	TMemo *memo;
	if (sdit == gutsSD)
	{
		memo = CalibrationSDMemo;
		LCxSD_out.resize(0);
	}
	else
	{
		memo = CalibrationITMemo;
		LCxIT_out.resize(0);
	}

	AnsiString as;
	as = "Results table for LCx,t [" + ConcUnit + "], with 95% CI";
	memo->Lines->Add(as);
	if (sdit == gutsSD)
		memo->Lines->Add("  Special case: stochastic death (SD)");
	else
		memo->Lines->Add("  Special case: individual tolerance (IT)");
	memo->Lines->Add("==================================================================");
	as = "Time [d]";
	for (int e = 0; e < predF_LCX_NO; e++) // run through effect levels requested
			as += "\tLC" + AnsiString((double)RoundTo(predF_LCX[e] * 100, 0)); // print table headers
	memo->Lines->Add(as);
	if (sdit == gutsSD)
		LCxSD_out.push_back(as);
	else
		LCxIT_out.push_back(as);
	memo->Lines->Add("==================================================================");
	for (int t = 0; t < predT_LCX_NO; t++) // run through times requested
	{
		as = AnsiString(predT_LCX[t]);
		for (int e = 0; e < predF_LCX_NO; e++) // run through effect levels requested
		{
			as += "\t" + MyFloatToStr(LCx[t][e], 4) + " (" + MyFloatToStr(LC_low[t][e], 4) +
				" - " + MyFloatToStr(LC_high[t][e], 4) + ")";
		}
		memo->Lines->Add(as);
		if (sdit == gutsSD)
			LCxSD_out.push_back(as);
		else
			LCxIT_out.push_back(as);
	}
	memo->Lines->Add("==================================================================");

	//// BLOCK 7. Finish the plot off with CIs.

	// Make an overall title for the plot with the filename, and save it.

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::Calc_LPx(vector<vector<double> > &LPx, vector<vector<double> > &LP_low, vector<vector<double> > &LP_high,
	vector<TPreparedTreatmentConc> &data_pred, int flag, int sdit, int stage, TMemo *memo, vector<AnsiString> &memo_string, AnsiString &err, const vector<TParameterSpace>& sample)
{



	// Matlab Usage: [LPx,LPlo,LPhi] = calc_lpx(data_pred,flag,sdit)
	//
	// Function to calculate LPx values, with confidence intervals. LPx is the
	// 'exposure-multiplication factor': the factor by which the exposure
	// profile needs to be multiplied to obtain a certain percentage mortality
	// after a specified duration. Output of LPx is needed as this is passed to
	// the plotting routine (to plot the LPx in the title of the plots). Output
	// of LPlo and LPhi would be for testing purposes only.
	//
	// I make use of the fact that the scaled damage level is proportional to
	// the exposure concentration. Multiplying an entire exposure profile is
	// thus equivalent to the multiplication of the damage levels over time by
	// the same factor. We therefore only need to calculate <Dw> once for every
	// parameter set. If we ever need to implement the full GUTS model (with
	// separate TK and damage) some shortcuts need to be reconsidered!
	//
	// Inputs
	// <data_pred> data object containing the exposure scenario (e.g., FOCUS profile)
	// <flag>      1) regular with CI on LPx, 2) no CI (very fast),
	// <stage>     stage of the analysis: PREDICTION or BATCH_PREDICTION
	// <sdit>      switch for SD (1) or IT (2)
	// Outputs
	// <LPx>    matrix with LPx values (nr of data sets x nr of effect levels)
	// <LPlo>   lower edge of the 95% CI; same format as <LPx>
	// <LPhi>   upper edge of the 95% CI; same format as <LPx>
	//
	// Author     : Tjalling Jager

	//// BLOCK 1. Initial things.

	string savestr;
	TParameterMatrix pmat;
	vector<double>parameter_set;
	vector<double>Sc;
	vector<double>maxDw;
	double unused_LCx, unused_LPx;
	TPreparedTreatmentConc ev_ic;
	double t_end;
	vector<double>S;
	vector<double>unused_S;
	int index_hi, index_lo;
	vector<double>mf_range;
	mf_range.resize(2);

	memo_string.resize(0);

	// tic // turn on the stopwatch to time this function

	//// BLOCK 2. Load calibration file as saved by <calc_parspace>.
	PMatSD.Value = PMatPrintSD.BestValue;
	PMatIT.Value = PMatPrintIT.BestValue;
	if (sdit == gutsSD)
	{
		pmat = PMatSD;
		LPxSD_out.resize(0);
	}
	else
	{
		pmat = PMatIT;
		LPxIT_out.resize(0);
	}
	pmat.Value[gutsHB] = 0.0; // set background hazard to zero and normal scale!
	pmat.Scale[gutsHB] = gutsNORM_SCALE_PARAM;
	parameter_set = pmat.Value;

	//// BLOCK 3. Calculate LPx and MF-survival for the best-fitting parameter set.
	// This block does two things: calculate MFs corresponding to a range of
	// survival probabilities (to make a sort of dose-response curve), and to
	// calculate the MF for specific values of survival (the LPx). Doing this
	// separately is practical for SD where we cannot directly calculate MFs
	// from a given survival probability.
	//
	// Note: there can now be multiple data sets (in the Matlab prototype), but
	// only one scenario per data set! For the software, we decided on one data
	// set for predictions, but it is good to be prepared.

	// BLOCK 3.1. Initial things. Initialise matrix to catch the LPx for the
	// best-fitting parameter set, and the CI.
	LPx.resize(data_pred.size());
	LP_low.resize(data_pred.size());
	LP_high.resize(data_pred.size());
	for (int d = 0; d < data_pred.size(); d++)
	{
		LPx[d].resize(predF_LPX_NO);
		LP_low[d].resize(predF_LPX_NO);
		LP_high[d].resize(predF_LPX_NO);

		for (int e = 0; e < predF_LPX_NO; e++)
		{
			LP_low[d][e] = HIGH_POS_DOUBLE;
			LP_high[d][e] = HIGH_NEG_DOUBLE;
		}
	}
	for (int d = 0; d < data_pred.size(); d++) // run through all data sets
	{
		Application->ProcessMessages();
		if (AbortPredictions)
			return;

		// BLOCK 3.2. Initial preparation before each data set.

		ev_ic = data_pred[d];
		// only extract first scenario from this data set (there should be only 1)
		t_end = ev_ic.Times[ev_ic.Times.size() - 1];
		// final time point for this scenario
		// Note: the final time is thus fixed to the last time point in the
		// exposure profile. If we ever decide to make this free (e.g., to use a
		// moving time window over the profile), this needs some modifications
		// in the code (especially below, in the calculation of <t_mod>).
		// Alternatively, for future time-window calculations, shortened <ev_ic>
		// matrices can be supplied to this function.

		vector<vector<double> >coll_survmf;
		coll_survmf.resize(predLEN_S_RANGE);
		for (int s = 0; s < coll_survmf.size(); s++)
		{
			coll_survmf[s].resize(4);				// initialise the matrix that will collect the survival-MF data, with its CI in column 3-4
			coll_survmf[s][2] = HIGH_POS_DOUBLE;	// the third column will collect the minimum (for the CI), so already initialise with INF
			coll_survmf[s][3] = HIGH_NEG_DOUBLE;
		}

		// Create a time vector for the calculations as <t_mod>.
		vector<double>t_mod;
			t_mod = EquidistantNumbers(0.0, t_end, t_end * TIME_POINTS_PER_DAY);
			t_mod.insert(t_mod.end(), ev_ic.Times.begin(), ev_ic.Times.end());
			sort(t_mod.begin(), t_mod.end());
			t_mod.erase(unique(t_mod.begin(), t_mod.end()), t_mod.end());

		// BLOCK 3.3. Start by calculating damage (<Dw>) for this scenario, for
		// multiplication factor 1
		TSimulationOutput Xout = Calc_Model(t_mod, parameter_set, ev_ic, sdit, err);
		if (err != "")
			return;
        if (StopBatchPredictions)
		{
			return;
		}
		// survival and damage at scenario <ev_ic>
		vector<double>Dw = Xout.ScaledDamage;
		// scaled damage levels are in the second column

		// BLOCK 3.4. For IT we can use a direct calculation: for each <S>, we
		// can ask for the corresponding MF.
		if (sdit == gutsIT) // for IT
		{
			// vector<double> maxDw;
			maxDw.resize(1);
			maxDw[0] = 0.0;
			for (int m = 0; m < Dw.size(); m++)
				if (Dw[m] > maxDw[0])
					// find the maximum value for <Dw> over time
						maxDw[0] = Dw[m];

			// Create a linear range of survival probabilities between the
			// values that we aim for for the survival-MF plot. Place this range
			// in the second column of <coll_survmf>.
			double a = predS_RANGE[0];
			double b = predS_RANGE[1];
			double n = predLEN_S_RANGE;
			double spacing;
			if (n - 1.0 > EPS && b - a > EPS)
				spacing = (b - a) / (n - 1.0);
			else
				spacing = 0.0;
			for (int m = 0; m < coll_survmf.size(); m++)
				coll_survmf[m][1] = a + m * spacing;
			for (int s = 0; s < predLEN_S_RANGE; s++) // run through this range
					Calc_Surv_IT(Sc, unused_LCx, coll_survmf[s][0], maxDw, parameter_set, 0, 1.0 - coll_survmf[s][1]);
			// calculate LPx in a sub-function
			// Note that x/100 = 1-S (at LP10, S=0.90). Each MF is placed in
			// the first column of <coll_survmf>.
			// Next, calculate LPx for the effect levels that we need to report.
			for (int e = 0; e < predF_LPX_NO; e++) // run through all effect levels
					Calc_Surv_IT(Sc, unused_LCx, LPx[d][e], maxDw, parameter_set, 0, predF_LPX[e]);
			// calculation of LPx in the sub-function
			// Note: both loops could have been avoided in the Matlab version as
			// the sub-function can be modified to handle input of a vector (and
			// returns a vector). However, using loops is probably more
			// transparent.
		}
		// BLOCK 3.5. For SD, we need to work the other way around. When we have
		// an MF, we can calculate the corresponding <S>. Since we want to know
		// the MF for the LPx, we need to use <fzero>. The strategy to follow is
		// to first go roughly, in steps of 10x, through the MFs to find out
		// where the desired range (<S_range>) can be found. We will refine later.
		else // For SD, some more work is needed ...
		{
			// BLOCK 3.5.1. Initial things.
			double mf_try = 1; // start with multiplication factor set to 1

			// No need to do the first one again, we already did that!
			S.resize(1);
			S[0] = Xout.SurvivalProb[Xout.SurvivalProb.size() - 1];
			// survival at the end of the profile for best parameter set
			vector<vector<double> >coll_rough;
			coll_rough.resize(1);
			// start a two-column matrix with the rough factors (elements will be added to it as we go along)
			coll_rough[0].push_back(mf_try);
			coll_rough[0].push_back(S[0]);
if (t_mod.size() != Dw.size())
	int test=0;
			// BLOCK 3.5.2. Next, roughly find the range of multiplication
			// factors where the survival probabilities in <S_range> are (take
			// steps of 10x).
			double d_temp;
			while (S[0] > predS_RANGE[0])
				// go up until survival is very low (beyond first element of range)
			{
				mf_try *= 10; // multiply last <mf_try> by factor of 10
				Calc_Surv_SD_t(d_temp, S, mf_try, parameter_set, t_mod, Dw, 1.0, err);
				if (err != "")
					return;
				// calculate survival probability at end of profile for new MF
				coll_rough.resize(coll_rough.size() + 1);
				// store new result in matrix, at bottom
				coll_rough[coll_rough.size() - 1].push_back(mf_try);
				coll_rough[coll_rough.size() - 1].push_back(S[0]);
			}
			mf_try = 1.0; // set MF back to 1
			S[0] = coll_rough[0][1]; // and return survival to first entry
			vector<vector<double> >coll_rough_temp;
			coll_rough_temp.resize(1);
			coll_rough_temp[0].resize(2);
			while (S[0] < predS_RANGE[1])
				// go down until survival is very high (beyond last element of range)
			{
				mf_try /= 10.0; // divide last <mf_try> by factor of 10
				Calc_Surv_SD_t(d_temp, S, mf_try, parameter_set, t_mod, Dw, 1.0, err);
				if (err != "")
					return;
				// calculate survival probability at end of profile for new MF
				coll_rough_temp[0][0] = mf_try;
				coll_rough_temp[0][1] = S[0];
				coll_rough.insert(coll_rough.begin(), coll_rough_temp.begin(), coll_rough_temp.end());
				// store new result in matrix, on top
			}

			// BLOCK 3.5.3. Use <fzero> to zoom in on exact place of survival
			// probabilities in <S_range>, first small one (5% survival by
			// default).

			// First find the range in <coll_rough> in which the small survival point is located.
			for (int k = 0; k < coll_rough.size(); k++)
				if (coll_rough[k][1] < predS_RANGE[0])
					// index to upper boundary for small survival
				{
					index_hi = k;
					break;
				}
			for (int k = coll_rough.size() - 1; k >= 0; k--)
				if (coll_rough[k][1] > predS_RANGE[0])
					// index to lower boundary for small survival
				{
					index_lo = k;
					break;
				}
			mf_range[0] = coll_rough[index_lo][0];
			// the small survival point is in this range of MFs
			mf_range[1] = coll_rough[index_hi][0];
			double mf_small;
			Setup_Calc_Surv_SD(parameter_set, t_mod, Dw, 1.0 - predS_RANGE[0]);
			ERR = "";
			mf_small = brent::zero(mf_range[0], mf_range[1], calibROOT_FINDING_TOL, Calc_Surv_SD_External);
            if (ERR != "")
					return;
			// mf_small = fzero(@calc_surv_sd_t,mf_range',[],pmat,t_mod,Dw,1-S_range(1)); // find the MF with <fzero>, from the reduced range

			// Next find the range in which the high survival point is located.
			// This follows the same priciple.
			for (int k = 0; k < coll_rough.size(); k++)
				if (coll_rough[k][1] < predS_RANGE[1])
					// index to upper boundary for high survival
				{
					index_hi = k;
					break;
				}
			for (int k = coll_rough.size() - 1; k >= 0; k--)
				if (coll_rough[k][1] > predS_RANGE[1])
					// index to lower boundary for high survival
				{
					index_lo = k;
					break;
				}
			mf_range[0] = coll_rough[index_lo][0];
			// the high survival point is in this range of MFs
			mf_range[1] = coll_rough[index_hi][0];
			double mf_large;
			Setup_Calc_Surv_SD(parameter_set, t_mod, Dw, 1.0 - predS_RANGE[1]);
			ERR = "";
			mf_large = brent::zero(mf_range[0], mf_range[1], calibROOT_FINDING_TOL, Calc_Surv_SD_External);
            if (ERR != "")
				return;
			// mf_large = fzero(@calc_surv_sd_t,mf_range',[],pmat,t_mod,Dw,1-S_range(2)); // find the MF with <fzero>, from the reduced range

			// BLOCK 3.5.4. Now, we can create a finer grid of multiplication
			// factors in the right spot.
			double a = log10(mf_large);
			double b = log10(mf_small);
			double n = predLEN_S_RANGE;
			double spacing;
			if (n - 1.0 > EPS && b - a > EPS)
				spacing = (b - a) / (n - 1.0);
			else
				spacing = 0.0;
			for (int m = 0; m < coll_survmf.size(); m++)
			{
				if (a + m * spacing < log10(HIGH_POS_DOUBLE))
					coll_survmf[m][0] = pow(10.0, a + m * spacing);
				else
					coll_survmf[m][0] = HIGH_POS_DOUBLE;
			}

			// Put the multiplication factors in the first column, log-spacing works better.
if (t_mod.size() != Dw.size())
	int test=0;
			for (int s = 0; s < predLEN_S_RANGE; s++) // run through the MF values to try
			{
				Calc_Surv_SD_t(d_temp, S, coll_survmf[s][0], parameter_set, t_mod, Dw, 1.0, err);
				if (err != "")
					return;
				// calculate the associated survival probability
				coll_survmf[s][1] = S[0];
				// collect it in the matrix in the second column
			}

			// BLOCK 3.5.5. Calculate the LPx values for the best parameter set,
			// for each specific effect level requested, using <fzero>.
			for (int e = 0; e < predF_LPX_NO; e++) // run through effect levels requested
			{
				// Use the initial range from the MF-survival matrix already
				// calculated to provide a narrow range for fzero.
				for (int k = 0; k < coll_survmf.size(); k++)
					if (coll_survmf[k][1] < 1.0 - predF_LPX[e])
						// index to upper boundary for this effect level
					{
						index_hi = k;
						break;
					}
				for (int k = coll_survmf.size() - 1; k >= 0; k--)
					if (coll_survmf[k][1] > 1.0 - predF_LPX[e])
						// index to lower boundary for this effect level
					{
						index_lo = k;
						break;
					}
				mf_range[0] = coll_survmf[index_lo][0];
				// the effect level is in this range of MFs
				mf_range[1] = coll_survmf[index_hi][0];

				Setup_Calc_Surv_SD(parameter_set, t_mod, Dw, predF_LPX[e]);
				ERR = "";
				LPx[d][e] = brent::zero(mf_range[0], mf_range[1], calibROOT_FINDING_TOL, Calc_Surv_SD_External);
                if (ERR != "")
					return;
				// LPx(d, e) = fzero(@calc_surv_sd_t,mf_range,[],pmat,t_mod,Dw,F_LPx(i_e)); // find the LPx with <fzero>, from the reduced range
			}
		}

		//// BLOCK 4. Output results for best-fitting parameter set.
		if (stage != BATCH_PREDICTION)
		{
			AnsiString as;
			as = "";
			if (flag < 3) // don't do this when we are doing batch processing
			{
				// BLOCK 4.1. Print LPx to screen.
				if (flag == 2) // print output to screen and diary
				{
					// This is now only done for cases where we want no CI. If we
					// want CIs, we'll have to wait until the calculations for the
					// CI are finished before we see the LPx. Depending on
					// calculation speed, we may want to reconsider this.

					as = "Results table for LPx [-]: data set " + AnsiString(d + 1);
					memo->Lines->Add(as);
					if (sdit == gutsSD)
					{
						memo->Lines->Add("  Special case: stochastic death (SD)");
						LPxSD_out.resize(0);
						LPxSD_out.push_back("LPx\tValue");
					}
					else
					{
						memo->Lines->Add("  Special case: individual tolerance (IT)");
						LPxIT_out.resize(0);
						LPxIT_out.push_back("LPx\tValue");
					}
					memo->Lines->Add("==================================================================");
					for (int e = 0; e < predF_LPX_NO; e++) // run through effect levels requested
					{
						as = "LP" + AnsiString((double)RoundTo(predF_LPX[e] * 100, 0)) + " at end of profile\t" +
							MyFloatToStr(LPx[d][e], 4);
						memo->Lines->Add(as);
						if (sdit == gutsSD)
							LPxSD_out.push_back(as);
						else
							LPxIT_out.push_back(as);
					}
					memo->Lines->Add("==================================================================");
				}

				// BLOCK 4.2. make a plot of survival vs. MF. If CIs are requested,
				// they will be plotted in the same figure later on.

				if (flag > 1)
					// then we don't need CIs, so make an overall title for the plot with the filename, and save it
				{
				}
			}
		}


		//// BLOCK 5. Calculate confidence intervals.
		// Use the saved sample from the parameters-space explorer, and take min
		// and max of results. Note: if we don't need CIs, we'll still run
		// through this code. However, the sample is made empty, so the <for>
		// loop won't even start.

		// BLOCK 5.1. Initial things

		parameter_set = pmat.Value;
		// make a copy of <pmat> as we will change it for each set in the sample
		// This is needed if we have multiple data sets to run through.

		double LPx_tmp;
		for (int n = 0; n < sample.size(); n++) // run through all sets in the sample
		{
			PredictionsProgressBar->Position = PredictionsProgressBar->Max;
			PredictionsProgressBar->Position = 100 * n / sample.size();
			Application->ProcessMessages();
			if (AbortPredictions)
				return;

			// BLOCK 5.2. Initial things for each set from the sample.
			parameter_set = sample[n].Parameter;
			// replace values in first column of <pmat> with the next set in the sample

			// Put parameters that need to be on log scale back on normal scale
			// (as the SD/IT calculations require normal scale).
			for (int p = 0; p < gutsPARAMETER_NO; p++)
				if (pmat.Scale[p] == gutsLOG_SCALE_PARAM)
				{
					if (parameter_set[p] < log10(HIGH_POS_DOUBLE))
						parameter_set[p] = pow(10.0, parameter_set[p]);
					else
						parameter_set[p] = HIGH_POS_DOUBLE;
				}
			// Note: <pmat> is on normal scale here, but the sample contains the
			// value on a log scale, if a parameter is fitted on log scale.

			Xout = Calc_Model(t_mod, parameter_set, ev_ic, sdit, err);
			if (err != "")
				return;
            if (StopBatchPredictions)
			{
				return;
			}
			// survival and damage at scenario <ev_ic>
			Dw = Xout.ScaledDamage;
			// scaled damage levels for this parameter set

			// BLOCK 5.3. For IT we can use a direct calculation of <S>.
			if (sdit == gutsIT) // for IT ...
			{
				maxDw[0] = 0.0;
				for (int w = 0; w < Dw.size(); w++)
					if (maxDw[0] < Dw[w])
						// find the maximum value for <Dw> over time
							maxDw[0] = Dw[w];

				// First the range of <S>-values for the survival-MF plot.
				S.resize(1);
				for (int mf = 0; mf < predLEN_S_RANGE; mf++) // run through the MF values to try
				{
					Calc_Surv_IT(S, unused_LCx, unused_LPx, maxDw, parameter_set, 0.0, -1.0, coll_survmf[mf][0]);
					// calculate survival probability in sub-function
					coll_survmf[mf][2] = Min(coll_survmf[mf][2], S[0]);
					// update the matrix in the third column (min CI)
					coll_survmf[mf][3] = Max(coll_survmf[mf][3], S[0]);
					// update the matrix in the fourth column (max CI)
				}

				// Next, the specific LPx values that we need to report.
				for (int e = 0; e < predF_LPX_NO; e++) // run through all effect levels
				{
					Calc_Surv_IT(unused_S, unused_LCx, LPx_tmp, maxDw, parameter_set, 0, predF_LPX[e], -1.0);
					// calculation of LPx in sub-function
					LP_low[d][e] = Min(LP_low[d][e], LPx_tmp);
					// keep the lowest value as minimum
					LP_high[d][e] = Max(LP_high[d][e], LPx_tmp);
					// keep the highest value as maximum
				}
			}

			// BLOCK 5.4. For SD, things are more complex (as it was for the
			// best parameter set). We won't do a rough search again, but only
			// use the range of MF values that we already established.
			else // for SD, some more work is needed ... need to use <fzero>
			{
				// BLOCK 5.4.1. Use the finer grid of multiplication factors
				// from the best fit again. Calculate the corresponding survival
				// for each set from the sample and keep highest/lowest.
				vector<double>coll_c;
				coll_c.resize(predLEN_S_RANGE);
				// initialise a vector to collect the <S> at each MF
				double unused_crit;
				for (int mf = 0; mf < predLEN_S_RANGE; mf++) // run through the MF values to try
				{
					Calc_Surv_SD_t(unused_crit, S, coll_survmf[mf][0], parameter_set, t_mod, Dw, 1.0, err);
					if (err != "")
						return;
					// calculate survival probability for this MF
					coll_survmf[mf][2] = Min(coll_survmf[mf][2], S[0]);
					// update the min in the matrix in the third column
					coll_survmf[mf][3] = Max(coll_survmf[mf][3], S[0]);
					// update the max in the matrix in the fourth column
					coll_c[mf] = S[0];
					// also collect the survival itself for zooming in on specific LPx values below
				}

				// BLOCK 5.4.2. Calculate the LPx values for this set, for each
				// effect level requested. Make use of the MF-S combinations
				// already calculated (<S> in <coll_c>, MF in first column of
				// <coll_survmf>).
				for (int e = 0; e < predF_LPX_NO; e++) // run through all effect levels
				{
					// Extract an initial range from the MF-survival matrix already calculated.
					index_hi = -1;
					index_lo = -1;
					for (int k = 0; k < coll_c.size(); k++)
						if (coll_c[k] < 1.0 - predF_LPX[e])
							// index to upper boundary for this effect level
						{
							index_hi = k;
							break;
						}
					for (int k = coll_c.size() - 1; k >= 0; k--)
						if (coll_c[k] > 1.0 - predF_LPX[e])
							// index to lower boundary for this effect level
						{
							index_lo = k;
							break;
						}
					/*mf_range[0] = coll_survmf[index_lo][0];
					// the effect level is in this range of MFs in first column
					mf_range[1] = coll_survmf[index_hi][0];
					// There can be cases when <coll_c> does not go low enough
					// to catch an LP10 for a set in the sample (i.e., that the
					// first entry in <coll_c> already has more than 10// effect).
					// That is no problem: the range will be a single value, and
					// <fzero> will search from there! (might be slow though).*/


					// NEW: There can be cases when <coll_c> does not go low
					// enough to catch an LP10 for a set in the sample (i.e.,
					// that the first entry in <coll_c> already has more than
					// 10// effect). That is no problem for <fzero>, however, it
					// is for the algorithm used in the C++ version. Therefore,
					// I try to find new bounds by changing the last one by a
					// factor of 10.
					double crit;
					vector<double> dummy_surv;
					if (index_lo != -1 && index_hi != -1) // then all is okay
					{
						mf_range[0] = coll_survmf[index_lo][0]; // the effect level is in this range of MFs in first column
						mf_range[1] = coll_survmf[index_hi][0];
					}
					if (index_lo == -1) // then no lower bound for MF was found
					{
						mf_range[0] = coll_survmf[0][0]; // start from first entry for lower bound
						mf_range[1] = coll_survmf[index_hi][0];

						crit = -1.0;
						while (crit < 0)
						{
							mf_range[0] /= 10.0; // divide first entry by 10 till it crosses the threshold
							Calc_Surv_SD_t(crit, dummy_surv, mf_range[0], parameter_set, t_mod,
									Dw, predF_LPX[e], err); // calculate new criterion for first entry in <mf_range>
							if (err != "")
								return;
						}
					}
					if (index_hi == -1) // then no upper bound for MF was found (which would be strange, but better safe than sorry)
					{
						if (index_lo == -1) // then they were both empty ... which would be really, really silly
							mf_range[1] = coll_survmf[coll_survmf.size()-1][0]; // then keep the first entry (which was filled above) and start from last value in <coll_surfmf>
						else
						{
							mf_range[0] = coll_survmf[index_lo][0];
							mf_range[1] = coll_survmf[coll_survmf.size()-1][0]; // start from last value in <coll_surfmf> for upper range
						}
						crit = 1.0;
						while (crit > 0)
						{
							mf_range[1] *= 10.0; // multiply second entry by 10 till it crosses the threshold
							Calc_Surv_SD_t(crit, dummy_surv, mf_range[1], parameter_set, t_mod,
									Dw, predF_LPX[e], err);	// calculate new criterion for second entry in <mf_range>
                            if (err != "")
								return;
						}
					}
					// END of NEW


					Setup_Calc_Surv_SD(parameter_set, t_mod, Dw, predF_LPX[e]);
					ERR = "";
					LPx_tmp = brent::zero(mf_range[0], mf_range[1], calibROOT_FINDING_TOL, Calc_Surv_SD_External);
					if (ERR != "")
						return;
					// LPx_tmp = fzero(@calc_surv_sd_t,mf_range,[],pmat_tmp,t_mod,Dw,F_LPx(i_e)); // find the LPx with <fzero>, from the reduced range
					LP_low[d][e] = Min(LP_low[d][e], LPx_tmp);
					// keep lowest value as minimum
					LP_high[d][e] = Max(LP_high[d][e], LPx_tmp);
					// keep highest value as maximum
				}
			}
		}

		//// BLOCK 6. Print results to screen and finish the plot with CIs.
		if (stage != BATCH_PREDICTION)
		{
			if (flag == 1)
			{
				// only for regular analyses with CI (for <flag>=2, the results without CIs have already been printed)
				if ( stage != BATCH_PREDICTION)
				{
					// BLOCK 6.1. Print to screen.
					AnsiString as;
					as = "Results table for LPx [-] with 95% CI: data set " + AnsiString(d + 1);
					memo->Lines->Add(as);
					if (sdit == gutsSD)
					{
						memo->Lines->Add("  Special case: stochastic death (SD)");
						LPxSD_out.resize(0);
						LPxSD_out.push_back("LPx\tValue");
					}
					else
					{
						memo->Lines->Add("  Special case: individual tolerance (IT)");
						LPxIT_out.resize(0);
						LPxIT_out.push_back("LPx\tBest value (CI)");
					}
					memo->Lines->Add("==================================================================");
					memo->Lines->Add("LPx\tBest value (CI)");
					memo->Lines->Add("==================================================================");
					for (int e = 0; e < predF_LPX_NO; e++) // run through effect levels
					{
						as = "LP" + AnsiString((double)RoundTo(predF_LPX[e] * 100, 0)) + "\t" + MyFloatToStr(LPx[d][e], 4)
							+ " (" + MyFloatToStr(LP_low[d][e], 4) + " - " + MyFloatToStr(LP_high[d][e], 4) + ")";
						memo->Lines->Add(as);
						if (sdit == gutsSD)
							LPxSD_out.push_back(as);
						else
							LPxIT_out.push_back(as);
					}
					memo->Lines->Add("==================================================================");
				}
			}
		}
		// BLOCK 6.2. Finish figure by plotting CIs in there.

		if (sdit == gutsSD)
			LPxSD = coll_survmf;
		else
			LPxIT = coll_survmf;

		// Make an overall title for the plot with the filename, and save it
	}



	return;
}
// ---------------------------------------------------------------------------

void TMainForm::Disp_PMat(TParameterMatrix &pmat, int sdit,
	TResultsMatrix &pmat_print, TMemo *memo, bool print_results)
{
	// Matlab Usage: disp_pmat(pmat,sdit,pmat_print)
	//
	// This function prints <pmat> and <pmat_print> on screen in a formatted
	// manner. This is the only place where the 5 model parameters and their CIs
	// are printed to screen.
	//
	// Inputs
	// <pmat>        parameter matrix
	// <sdit>        switch for SD (1) or IT (2)
	// <pmat_print>  matrix with information on each parameter (best value and CI info)
	//
	// Author     : Tjalling Jager

	// Define strings for the printing units of the parameters.
	vector<AnsiString>units;
	units.resize(gutsPARAMETER_NO);
	units[gutsKD] = "1/d";
	units[gutsMW] = ConcUnit;
	units[gutsBW] = "1/(" + ConcUnit + " d)";
	units[gutsHB] = "1/d";
	units[gutsFS] = "[-]";

	AnsiString as;
	as = "";
	AnsiString logsc;

	//// Block 2. Print parameter values/ranges/intervals.

	if (!print_results)
		// then we only print bounds (before an optimisation run)
	{
		memo->Lines->Add("=====================================================================");
		for (int p = 0; p < gutsPARAMETER_NO; p++) // run through parameters
		{
			if ((sdit == gutsSD && p != gutsFS) || (sdit == gutsIT && p != gutsBW))
			{
				if (pmat.Scale[p] == gutsLOG_SCALE_PARAM)
					// create a string for log or normal scale fitting
						logsc = "log";
				else
					logsc = "norm";
				as = AnsiString(gutsPARAMETERS[p].c_str()) + " bounds: " + MyFloatToStr(pmat.Min[p], 4) + " - " +
					MyFloatToStr(pmat.Max[p], 4) + " " + units[p] + ", fit: " + AnsiString(pmat.Fit[p] == gutsFIT_PARAM ? "Yes" : "No")
					+ " (" + logsc + ")";
				memo->Lines->Add(as);
			}
		}
		memo->Lines->Add("=====================================================================");
	}
	else // then we print the results of a calibration run and more info needs to be printed
	{
		memo->Lines->Add("==========================================================================");
		for (int p = 0; p < gutsPARAMETER_NO; p++) // run through parameters
		{
			if ((sdit == gutsSD && p != gutsFS) || (sdit == gutsIT && p != gutsBW))
			{
				if (pmat.Scale[p] == gutsLOG_SCALE_PARAM)
					// create a string for log or normal scale fitting
						logsc = "log";
				else
					logsc = "norm";
				// Print the line to screen (broken up in three command lines).
				if (pmat.Fit[p] == gutsFIT_PARAM)
					as = AnsiString(gutsPARAMETERS[p].c_str()) + " best:  " + MyFloatToStr(pmat_print.BestValue[p], 4) + " (" +
						MyFloatToStr(pmat_print.LowerCI[p], 4) + AnsiString(pmat_print.LowerCIWarning[p].c_str()) + " - " +
						MyFloatToStr(pmat_print.UpperCI[p], 4) + AnsiString(pmat_print.UpperCIWarning[p].c_str()) + ") " + units[p] +
						", scale: " + logsc;
				else
					as = AnsiString(gutsPARAMETERS[p].c_str()) + " input: " + MyFloatToStr(pmat_print.BestValue[p], 4) + " " +
						units[p];
				memo->Lines->Add(as);
			}
		}
		memo->Lines->Add("==========================================================================");
		memo->Lines->Add("* edge of 95% parameter CI has run into a boundary");
		memo->Lines->Add("  (this may also have affected CIs of other parameters)");
	}

	if (sdit == gutsSD) // also print the special case we have just printed
		memo->Lines->Add("Special case: stochastic death (SD)");
	else
		memo->Lines->Add("Special case: individual tolerance (IT)");
	if (PrefitHBToControl) // did we fix <hb> to the controls?
		memo->Lines->Add("Background hazard rate fixed to a value fitted on controls");

	//// Block 3. After calibration, we need to print some additional information

	if (print_results)
		// then we print the results of a calibration run and more info needs to be printed
	{
		memo->Lines->Add("Extra information from the fit:");
		memo->Lines->Add("==========================================================================");

		for (int p = 0; p < gutsPARAMETER_NO; p++) // run through parameters
			if (pmat.Fit[p] == gutsFIT_PARAM)
				if (pmat_print.Broken[p]) // is there a flag for a broken CI?
				{
					as = "Confidence interval for parameter " + AnsiString(gutsPARAMETERS[p].c_str()) + " is a broken set.";
					memo->Lines->Add(as);
				}

		if (sdit == gutsIT && pmat.Fit[gutsFS] == gutsFIT_PARAM)
			// for IT, also show beta of the log-logistic distribution
		{
			AnsiString beta_as, beta_lo_as, beta_hi_as;
			if (abs(pmat_print.BestValue[gutsFS] - 1.0) < EPS)
				beta_as = "Inf";
			else
				beta_as = MyFloatToStr(log(39.0) / log(pmat_print.BestValue[gutsFS]), 4); // translate <Fs> to beta
			// also translate CI (note that low <Fs> means high beta)
			if (abs(pmat_print.UpperCI[gutsFS] - 1.0) < EPS)
				beta_lo_as = "Inf";
			else
				beta_lo_as = MyFloatToStr(log(39.0) / log(pmat_print.UpperCI[gutsFS]), 4);
			if (abs(pmat_print.LowerCI[gutsFS] - 1.0) < EPS)
				beta_hi_as = "Inf";
			else
				beta_hi_as = MyFloatToStr(log(39.0) / log(pmat_print.LowerCI[gutsFS]), 4);
			as = "Beta of the threshold distribution: " + beta_as + " (" + beta_lo_as + " - " + beta_hi_as + ") [-]";
			memo->Lines->Add(as);
		}

		if (pmat.Fit[gutsKD] == gutsFIT_PARAM)
		{
			// Calculate and print the DRT95 from <kd>.
			AnsiString DRT95, DRT95lo, DRT95hi;
			if (pmat_print.BestValue[gutsKD] > EPS)
				DRT95 = MyFloatToStr(log(20.0) / pmat_print.BestValue[gutsKD], 4);
			// calculate DRT95 from <kd>
			else
				DRT95 = "Inf";
			if (pmat_print.UpperCI[gutsKD] > EPS)
				DRT95lo = MyFloatToStr(log(20.0) / pmat_print.UpperCI[gutsKD], 4);
			// and its CI
			else
				DRT95lo = "Inf";
			if (pmat_print.LowerCI[gutsKD] > EPS)
				DRT95hi = MyFloatToStr(log(20.0) / pmat_print.LowerCI[gutsKD], 4);
			else
				DRT95hi = "Inf";
			as = "Depuration/repair time (DRT95)    : " + DRT95 + " (" + DRT95lo + " - " + DRT95hi + ") days";
			memo->Lines->Add(as);
		}

		// Due to popular demand: print the parameter search ranges again ...
		memo->Lines->Add("Parameter ranges used for the optimisation:");
		for (int p = 0; p < gutsPARAMETER_NO; p++) // run through parameters
			if (pmat.Fit[p] == gutsFIT_PARAM)
			{
				as = AnsiString(gutsPARAMETERS[p].c_str()) + " range: " + MyFloatToStr(pmat.Min[p], 4) + " - " +
					MyFloatToStr(pmat.Max[p], 4);
				memo->Lines->Add(as);
			}
		memo->Lines->Add("==========================================================================");
	}

	return;
}
// ---------------------------------------------------------------------------

double Calc_Surv_SD_External(double input_variable)
{
if (MainForm->Calc_Surv_SD_Params.Time.size() != MainForm->Calc_Surv_SD_Params.Dw.size())
	int test=0;

	if (MainForm->Calc_Surv_SD_Params.Calc_Surv_Function == predCALC_SURV_SD_C)
		MainForm->Calc_Surv_SD_c(MainForm->Calc_Surv_SD_Params.Crit, MainForm->Calc_Surv_SD_Params.Sc, input_variable,
		MainForm->Calc_Surv_SD_Params.Parameter_Set, MainForm->Calc_Surv_SD_Params.Time,
		MainForm->Calc_Surv_SD_Params.Fraction_Effect, ERR);
	else
		MainForm->Calc_Surv_SD_t(MainForm->Calc_Surv_SD_Params.Crit, MainForm->Calc_Surv_SD_Params.Sc, input_variable,
		MainForm->Calc_Surv_SD_Params.Parameter_Set, MainForm->Calc_Surv_SD_Params.Time, MainForm->Calc_Surv_SD_Params.Dw,
		MainForm->Calc_Surv_SD_Params.Fraction_Effect, ERR);

	return MainForm->Calc_Surv_SD_Params.Crit;
}
// ---------------------------------------------------------------------------

void TMainForm::Setup_Calc_Surv_SD(vector<double> &parameter_set, vector<double> &time, double fraction_effect)
	// setup for Calc_Surv_SD_c
{
	Calc_Surv_SD_Params.Crit = 0.0; // will be changed (call by reference) later
	Calc_Surv_SD_Params.Sc.resize(0);
	// will be changed (call by reference) later
	Calc_Surv_SD_Params.Parameter_Set = parameter_set;
	Calc_Surv_SD_Params.Time = time;
	Calc_Surv_SD_Params.Dw.resize(0);
	// not needed for Calc_Surv_SD_c, only for Calc_Surv_SD_t
	Calc_Surv_SD_Params.Fraction_Effect = fraction_effect;
	Calc_Surv_SD_Params.Calc_Surv_Function = predCALC_SURV_SD_C;
	// marks the object as corresponding to Calc_Surv_SD_c

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::Setup_Calc_Surv_SD(vector<double> &parameter_set, vector<double> &time, vector<double> &dw,
	double fraction_effect)
	// setup for Calc_Surv_SD_t (includes setup for Dw)
{
	Calc_Surv_SD_Params.Crit = 0.0; // will be changed (call by reference) later
	Calc_Surv_SD_Params.Sc.resize(0);
	// will be changed (call by reference) later
	Calc_Surv_SD_Params.Parameter_Set = parameter_set;
	Calc_Surv_SD_Params.Time = time;
	Calc_Surv_SD_Params.Dw = dw;
	Calc_Surv_SD_Params.Fraction_Effect = fraction_effect;
	Calc_Surv_SD_Params.Calc_Surv_Function = predCALC_SURV_SD_T;
	// marks the object as corresponding to Calc_Surv_SD_t

	return;
}
// ---------------------------------------------------------------------------

bool TMainForm::Calc_Optim(TResultsMatrix &pmat_print, double &mll,
	TParameterMatrix &pmat, int sdit, int flag, AnsiString &err)
{


	// Matlab Usage: [pmat_print,mll] = calc_optim(pmat,sdit,flag)
	//
	// In principle, no output would be needed. However, for some forms of
	// testing, output is handy. This function is a shell around the calibration
	// algorithm in <calc_parspace>. It prepares for the optimisation, takes
	// care of a restart (when slow kinetics is indicated during the
	// optimisation), and it plots parameter space for saved sets if needed.
	//
	// Optimisation and finding a likelihood-based joint confidence region
	// without the need for starting values. However, the parameter space to be
	// searched must limited for the algorithm to be effective. Parameter space
	// to be searched is defined as the min-max bounds of the parameters in the
	// matrix <pmat>.
	//
	// Note that the function <disp_pmat> is called to do the formatted printing
	// (on screen) of <pmat> and <pmat_print>. The <pmat> is displayed before
	// optimisation (to show the search ranges), the <pmat_print> after (with
	// the optimised values and CIs).
	//
	// Inputs
	// <pmat>        parameter matrix (5x5 with search ranges, fit/fix, etc.)
	// <sdit>        switch for SD (1) or IT (2)
	// <flag>        1) regular optimisation, 2) use saved set
	//
	// Outputs
	// <pmat_print>  matrix with parameter info (best fit, CIs, etc.)
	// <mll>         minus log-likelihood for the fit
	//
	// Author     : Tjalling Jager

	TMemo *memo;
	if (sdit == gutsSD)
		memo = CalibrationSDMemo;
	else
		memo = CalibrationITMemo;
	AnsiString as;

	//// BLOCK 1. Use the saved set, and don't make a new calculation.

	if (flag == calibUSE_EXISTING_PARAM_SAMPLE)
	{
		// And display it to screen and diary.
		int n_fit = 0; // number of fitted parameters
		for (int p = 0; p < gutsPARAMETER_NO; p++)
			if (pmat.Fit[p] == gutsFIT_PARAM)
				n_fit++;
		memo->Lines->Add("=================================================================================");
		memo->Lines->Add("Displaying and plotting optimisation results from a saved set");
		memo->Lines->Add("=================================================================================");
		if (sdit == gutsSD)
		{
			CalibrationSDGoodnessOfFit.MLL = AnsiString((double)RoundTo(mll, -2));
			CalibrationSDGoodnessOfFit.AIC = AnsiString((double)RoundTo(2 * n_fit + 2 * mll, -2));
		}
		else
		{
			CalibrationITGoodnessOfFit.MLL = AnsiString((double)RoundTo(mll, -2));
			CalibrationITGoodnessOfFit.AIC = AnsiString((double)RoundTo(2 * n_fit + 2 * mll, -2));
		}
		as = "   Minus log-likelihood: " + AnsiString((double)RoundTo(mll, -2)) + " AIC: " + AnsiString((double)RoundTo(2 * n_fit + 2 * mll, -2));
		memo->Lines->Add(as);
		memo->Lines->Add("Best fit parameter set and 95% bounds from the saved set:");

		Disp_PMat(pmat, sdit, pmat_print, memo, true);
		// call dedicated function for printing the <pmat> and <pmat_print>
		return true; // nothing further to do, so return to the main script
	}

	//// BLOCK 2. Call <calc_parspace> to do an optimisation.
	for (int p = 0; p < gutsPARAMETER_NO; p++)
		if (pmat.Scale[p] == gutsLOG_SCALE_PARAM && pmat.Value[p] <= 0.0)
		{
			ShowMessage("For log parameters, lower bound cannot be zero. Check the parameter structure.");
			return false;
		}
	// This check is best performed when people are modifying <pmat> in the
	// 'expert setting'.

	memo->Lines->Add("Settings for parameter search ranges:");
	Disp_PMat(pmat, sdit, pmat_print, memo, false);
	// call <disp_pmat> for a formatted printing of the parameter search ranges

	// Call the optimisation function <calc_parspace> to do the optimisation.
	// [minmax,mll,stats,pmat_print] = calc_parspace(pmat,sdit);
	// parameters for Calc_Parspace():
	TParameterMatrix minmax;
	/* this is used to restart when slow kinetics is indicated; in
	 %    that case, it is a matrix with the min/max for each parameter of the
	 %    cloud that was used for testing slow kinetics. If all goes well,
	 %    <minmax>=-1 and no restart is made. */
	// first we need to initialise the pseudo parameter matrix we use for minmax:
	minmax.Value.resize(1); // first value used as identifier (=-1) for restart
	minmax.Min.resize(gutsPARAMETER_NO);
	minmax.Max.resize(gutsPARAMETER_NO);
	vector<int>stats;
	// some statistics from the run: vector with number of sets in joint CI, inner rim, and propagation band
	Calc_Parspace(minmax, mll, stats, pmat_print, pmat, sdit, err);
	if (err != "")
		return false;
	if (AbortCalibration)
		return true;

	// The <calc_parspace> will break the analysis if it runs into slow kinetics
	// and when <mw> is estimated on normal scale. In that case, it needs to
	// restart with <mw> on log-scale, and additionally, the parameter ranges
	// can probably be decreased a bit.
	if (minmax.Value[0] != -1)
		// then we stopped before the end ... this should only happen once (so no 'while' needed)
	{
		// We suspect slow kinetics (currently the only reason for a restart).
		memo->Lines->Add("Slow kinetics indicated: parameter-space explorer is restarting with these settings:");
		// <calc_parspace> returned the min-max of (relevant part of) parameter
		// cloud on normal scale.
		pmat.Scale[gutsMW] = gutsLOG_SCALE_PARAM;
		// for <mw>, put fit flag on log scale
		// Note: if someone *really* wants to keep <mw> on normal scale, there
		// is currently no simple way to avoid triggering a restart and a
		// log-scale for <mw>.

		// Note: for <kd>, we don't need to put it on log scale, as it already
		// is (see <calc_parspace>: if <kd> is on normal scale, there will be no
		// check on slow kinetics anyway, so no restart).

		// Adapt parameter bounds in <pmat> where possibel, using the values in <minmax>.
		pmat.Max[gutsMW] = Min(pmat.Max[gutsMW], minmax.Max[gutsMW] * optimSLOWKIN_F_MW);
		// if possible, adapt upper boundary of <mw>
		pmat.Max[gutsKD] = Min(pmat.Max[gutsKD], minmax.Max[gutsKD] * optimSLOWKIN_F_KD);
		// also modify upper bound <kd> a bit if possible ...
		if (sdit == gutsSD) // only for SD
				pmat.Min[gutsBW] = Max(pmat.Min[gutsBW], minmax.Min[gutsBW] / 3.0);
		// also modify lower boundary <bw> a bit if possible ...
		else
		{
			// also modify bounds on <Fs>? Probably not needed
		}
		// The modifications of the upper boundaries for <mw> and <kd>, and
		// lower boundary for <bw>, are bit tricky but seem to work ...

		// Display new to-be-fitted parameter ranges on screen.
		Disp_PMat(pmat, sdit, pmat_print, memo, false);
		// call dedicated function for printing the <pmat>

		// And restart the optimisation with <calc_parspace>.
		Calc_Parspace(minmax, mll, stats, pmat_print, pmat, sdit, err);
		if (err != "")
			return false;
        if (AbortCalibration)
			return true;
	}


	//// BLOCK 3. Final printing on screen.
	// CalibrationPerformed = true; // indicator for Disp_PMat() to display pmat_print instead of pmat

	// The stats vector contains some interesting info from the analysis.
	int ind_final = stats[0];
	// index to the last element in the sample in the joint 95% CI
	int ind_inner = stats[1];
	// index to the last element in the sample in the inner rim
	int ind_prop = stats[2];
	// number of elements in the sample that are used for propagating errors

	memo->Lines->Add("=================================================================================");
	memo->Lines->Add("Results of the parameter estimation");
	memo->Lines->Add("=================================================================================");
	int n_fit = 0; // number of fitted parameters
	for (int p = 0; p < gutsPARAMETER_NO; p++)
		if (pmat.Fit[p] == gutsFIT_PARAM)
			n_fit++;
	as = "   Sample: " + AnsiString(ind_final + 1) + " sets in joint CI and " + AnsiString(ind_inner + 1) + " in inner CI.";
	memo->Lines->Add(as);
	as = "   Propagation set: " + AnsiString(ind_prop + 2) + " sets will be used for error propagation.";
	memo->Lines->Add(as);
	// Display minloglik and Akaike Information Criterion.
    if (sdit == gutsSD)
	{
		CalibrationSDGoodnessOfFit.MLL = AnsiString((double)RoundTo(mll, -2));
		CalibrationSDGoodnessOfFit.AIC = AnsiString((double)RoundTo(2 * n_fit + 2 * mll, -2));
	}
	else
	{
		CalibrationITGoodnessOfFit.MLL = AnsiString((double)RoundTo(mll, -2));
		CalibrationITGoodnessOfFit.AIC = AnsiString((double)RoundTo(2 * n_fit + 2 * mll, -2));
	}
	as = "   Minus log-likelihood has reached " + AnsiString((double)RoundTo(mll, -2)) + " (AIC=" + AnsiString((double)RoundTo(2.0 * n_fit + 2.0 * mll, -2)) + ").";
	memo->Lines->Add(as);

	memo->Lines->Add("Best estimates and 95% CIs on single parameters");
	Disp_PMat(pmat, sdit, pmat_print, memo, true);
	// call dedicated function for printing the <pmat>



	return true;
}
// ---------------------------------------------------------------------------

void TMainForm::SortParameterSpaceList(vector<TParameterSpace> &param_space)
	// sorts a 'parameter space' vector containing parameter values (vector) and loglikelihood values (double)
{
	if (param_space.size() < 2)
		return;

	vector<pair<double, vector<double> > >sort_vect;
	// temporary pair vector used for sorting
	sort_vect.resize(param_space.size());
	for (int s = 0; s < sort_vect.size(); s++)
	{
		sort_vect[s].first = param_space[s].Likelihood;
		sort_vect[s].second = param_space[s].Parameter;
	}
	sort(sort_vect.begin(), sort_vect.end());
	// sort pair vector     // sort based on the minloglik in the last column (keep parameter sets together)
	for (int s = 0; s < sort_vect.size(); s++)
	{
		param_space[s].Likelihood = sort_vect[s].first;
		param_space[s].Parameter = sort_vect[s].second;
	}

	return;
}
// ---------------------------------------------------------------------------

vector<TParameterSpace> TMainForm::Rand_Mutations(TParameterMatrix &pmat,
		vector<TParameterSpace> &coll_ok, int sdit, TParameterMatrix &bnds_tmp,
		int n_tr_i, vector<double> &d_grid_i, AnsiString &err)
{
	// Matlab Usage: coll_tries = rand_mutations(pmat,coll_ok,sdit,bnds_tmp,n_tr_i,d_grid_i)
	//
	// This function randomly mutates the parameter sets in <coll_ok>, with the
	// settings obtained for this round from <calc_parspace>. For each mutated
	// set, it calculates the min-log-likelihood (MLL). Mutated sets with their
	// MLL are returned in the matrix <coll_tries>.
	//
	// The inputs may be organised in a different form. Quite a few inputs are
	// needed, and it is only called from <calc_parspace>. However, this part of
	// the code is called at two locations in <calc_parspace>, and is therefore
	// best placed in a separate function.
	//
	// Inputs
	// <pmat>     parameter matrix
	// <coll_ok>  parameter sets (and their MLL) that need to be mutated to new tries (matrix)
	// <sdit>     switch for SD (1) or IT (2)
	// <bnds_tmp> boundaries of parameter space (log-transformed were needed) (matrix)
	// <n_tr_i>   number of new parameter tries per element of <coll_ok>
	// <d_grid_i> initial grid spacing for each parameter times the max jump size in this round (vector)
	// Note: the function call in <calc_parspace> produces this entry
	// as <f_d_i*d_grid>.
	//
	// Outputs
	// <coll_tries>  mutated parameter sets with their MLL (matrix)
	//
	// Author     : Tjalling Jager



	//// BLOCK 1. Initial things.
	vector<TParameterSpace>coll_tries;
	vector<vector<double> >p_try;
	p_try.resize(gutsPARAMETER_NO);
	double mll_tst;
	vector<double>pfit;
	int fit_count;
	double rand_num;

	vector<int>ind_fit; // logical indices to fitted parameters (vector)
	ind_fit.resize(0);
	for (int p = 0; p < gutsPARAMETER_NO; p++)
		if (pmat.Fit[p] == gutsFIT_PARAM)
			ind_fit.push_back(p);
	// n_cont     = size(coll_ok,1); // how many sets to mutate with
	// coll_tries.resize(coll_ok.size() * n_tr_i);
	// for (int i=0; i<coll_tries.size(); i++)
	// coll_tries[i].Likelihood = HIGH_POS_DOUBLE; // initialise matrix (with INFs) to catch all new sets and their MLL

	//// BLOCK 2. Fill coll_tries.
	// This section loops over all parameter sets in <coll_ok>. The first
	// sub-loop creates a series of mutated parameter sets, and the second
	// sub-loop runs it through <transfer> to obtain an MLL.

	coll_tries.resize(0);
	for (int ok = 0; ok < coll_ok.size(); ok++) // run through the ok parameter sets that were entered
	{
		// Make new tries out of this parameter set, with a random mutation for
		// each parameter, the mutation jumps at max a factor <f_d_i> of the
		// initial grid spacing in round 1 (max jump distance for each parameter
		// in this round is already integrated in the vector <d_grid_i>).

        // check for abortion
		Application->ProcessMessages();
		if (AbortCalibration)
		{
			//err = "Calibration aborted.";
			return coll_tries;
		}

		// BLOCK 2.1. Generate a matrix with mutated parameter sets for this
		// entry in <coll_ok>.
		for (int i = 0; i < p_try.size(); i++)
			p_try[i].resize(0);
		fit_count = 0;
		for (int p = 0; p < gutsPARAMETER_NO; p++) // run through all parameters
		{
			if (pmat.Fit[p] == gutsFIT_PARAM) // are we fitting parameter <i_p>?
			{
				for (int i = 0; i < n_tr_i; i++) // Create a vector with <n_tr_i> new parameter values to try, and place in correct column of <p_try>.
				{
					rand_num = (double)rand() / (double)RAND_MAX;
					p_try[p].push_back(coll_ok[ok].Parameter[p] + (rand_num * 2.0 - 1.0)
						* d_grid_i[fit_count]);
					// add a random number between -1 and 1, multiplied by max jump
					p_try[p][i] = Max(p_try[p][i], bnds_tmp.Min[p]);
					// make sure that all new tries are within min bound
					p_try[p][i] = Min(p_try[p][i], bnds_tmp.Max[p]);
					// make sure that all new tries are within max bound

				}
				fit_count++;
			}
			else
				for (int i = 0; i < n_tr_i; i++)
				{
					p_try[p].push_back(pmat.Value[p]);

				}
			// otherwise, fill the column of <p_try> with only the fixed value in <pmat>

		}

		// BLOCK 2.2. Calculate a minus-log-likelihood for the mutated values in
		// <p_try>, and add the parameters and MLL to <coll_tries>.
		TParameterSpace params_tmp;
		params_tmp.Parameter.resize(gutsPARAMETER_NO);
		for (int t = 0; t < n_tr_i; t++) // run through all new random tries for this entry in <coll_ok>
		{
			pfit.resize(0);
			for (int p = 0; p < ind_fit.size(); p++)
				pfit.push_back(p_try[ind_fit[p]][t]);
			// put new tries in vector <pfit> for parameters that are to be fitted
			mll_tst = Transfer(pfit, pmat, sdit, err);
			if (err != "")
				return coll_tries;
			// calculate the min-log-likelihood for this parameter combination
			for (int p = 0; p < gutsPARAMETER_NO; p++)
				params_tmp.Parameter[p] = p_try[p][t];
			params_tmp.Likelihood = mll_tst;
			coll_tries.push_back(params_tmp);
			// and collect it with the parameter values in <coll_tries>
		}
	}


	//// BLOCK 3. Prepare <coll_tries> and sort.



	for (int i = 0; i < coll_tries.size(); i++)
		if (coll_tries[i].Likelihood > HIGH_POS_DOUBLE - 1)
			// remove the ones that are INF for the MLL
		{
			coll_tries[i] = coll_tries[coll_tries.size() - 1];
			coll_tries.resize(coll_tries.size() - 1);
			i--;
		}
	SortParameterSpaceList(coll_tries);
	// sort based on MLL (keep rows/sets together)



	return coll_tries;
}
// ---------------------------------------------------------------------------

double TMainForm::CalcCorrCoef(vector<double> &x, vector<double> &y)
	// calculate the correlation coefficient of two vectors
{
	if (x.size() != y.size())
		return 0.0;

	double corr_coef;
	int n = x.size();
	double x_mean, y_mean, x_sum, y_sum;
	double xx, yy, xy;
	x_sum = 0.0;
	y_sum = 0.0;
	for (int i = 0; i < n; i++)
	{
		x_sum += x[i];
		y_sum += y[i];
	}
	x_mean = x_sum / n;
	y_mean = y_sum / n;

	xx = 0.0;
	yy = 0.0;
	xy = 0.0;
	for (int i = 0; i < n; i++)
	{
		xy += (x[i] - x_mean) * (y[i] - y_mean);
		xx += (x[i] - x_mean) * (x[i] - x_mean);
		yy += (y[i] - y_mean) * (y[i] - y_mean);
	}
	xx = pow(xx, 0.5);
	yy = pow(yy, 0.5);

	corr_coef = xy / (xx * yy);

	return corr_coef;
}
// ---------------------------------------------------------------------------

void TMainForm::Calc_Parspace(TParameterMatrix &minmax, double &mll, vector<int> &stats, TResultsMatrix &pmat_print,
	TParameterMatrix pmat, int sdit, AnsiString &err)
{
	// Matlab Usage: [minmax,mll,stats,pmat_print] = calc_parspace(pmat,sdit)
	//
	// Optimisation and finding a likelihood-based joint confidence region
	// without the need for starting values. However, the parameter space to be
	// searched must limited for the algorithm to be effective. Parameter space
	// to be searched is defined as the min-max bounds of the parameters in the
	// matrix <pmat>.
	//
	// Inputs
	// <pmat>        parameter matrix
	// <sdit>        switch for SD (1) or IT (2)
	//
	// Outputs
	// <minmax>  this is used to restart when slow kinetics is indicated; in
	// that case, it is a matrix with the min/max for each parameter of the
	// cloud that was used for testing slow kinetics. If all goes well,
	// <minmax>=-1 and no restart is made.
	// <mll>     minus log-likelihood; this will be the lowest value found in the run
	// <stats>   some statistics from the run: vector with number of sets in
	// joint CI, inner rim, and propagation band
	// <pmat_print>  matrix with information on each parameter (best value and CI info)
	//
	// Author     : Tjalling Jager

	//// BLOCK 1. Initial things and save when no parameters are fitted

	// BLOCK 1.1. Make sure a few pieces of info are available if we want to
	// save. We need to save very rapidly when there are no parameters to be
	// fitted at all.





	// define target of user output:
	TMemo *memo;
	if (sdit == gutsSD)
		memo = CalibrationSDMemo;
	else
		memo = CalibrationITMemo;

	// define vars:
	AnsiString as;
	AnsiString savestr;
	vector<TParameterSpace>coll_all; // sample from parameter space
	vector<TParameterSpace>coll_ok;
	vector<TParameterSpace>coll_tst;
	TParameterMatrix bnds_tmp;
	// temporary matrix only containing parameter bounds
	vector<double>unused_pfit;
	// just an empty temporary vector for calling Transfer()
	vector<double>pfit; // vector containing values of parameters to fit
	vector<double>phat;
	int flag_restart;
	vector<double>flag_profile; // output from test_proflik
	flag_profile.resize(2);
	int flag_short; // output from calc_proflik
	vector<vector<TParameterSpace> >coll_prof;
	// output from calc_proflik; first vector for each parameter, second vector for each profile point over the parameter range
	vector<vector<TParameterSpace> >coll_prof_pruned;
	// pruned version of coll_prof
	stats.resize(3);

	if (sdit == gutsSD)
		// create the string for the filename of the MAT file to be saved
			savestr = "_parspace_SD";
	else
		savestr = "_parspace_IT";

	// BLOCK 1.2. // Derive several useful properties of <pmat>.
	int n_fit = 0; // number of fitted parameters
	// int n_par = gutsPARAMETER_NO;   // number of parameters in total
	vector<int>ind_fit, ind_log;
	// indices to fitted and log-scale parameters (vectors)
	ind_fit.resize(0);
	ind_log.resize(0);
	for (int p = 0; p < gutsPARAMETER_NO; p++)
	{
		if (pmat.Fit[p] == gutsFIT_PARAM)
		{
			ind_fit.push_back(p);
			n_fit++;
		}
		if (pmat.Scale[p] == gutsLOG_SCALE_PARAM)
			ind_log.push_back(p);
	}
	minmax.Value[0] = -1;
	// = -1; // set to -1, so by default <calc_optim> thinks all is okay
	// set first 'Value' of TParameterMatrix minmax to -1 as identifier

	// BLOCK 1.3. When no parameters are fitted, we need to do something. In
	// principle, someone can fix all parameters and just run that (as
	// simulation). That is fine, but since the plotting and post calculations
	// all depart from a saved calibration file, we need to create one without
	// going through the optimisation.
	if (ind_fit.size() == 0) // no fitted parameters at all!
	{
		coll_all.resize(0); // make empty sample from parameter space
		coll_prof_pruned.resize(0); // make empty profile likelihoods
		stats[0] = 0.0;
		// fill the statistics (for <calc_optim> to print) with zeros
		stats[1] = 0.0;
		stats[2] = 0.0;

		pmat_print.BestValue = pmat.Value;
		// copy the input parameter values to the first column of <pmat_print>

		Disp_PMat(pmat, sdit, pmat_print, memo, false);
		// display pmat in a formatted manner on screen

		for (int p = 0; p < ind_log.size(); p++)
			pmat.Value[ind_log[p]] = log10(pmat.Value[ind_log[p]]);
		// put parameters that need to be on log scale on log10 scale
		unused_pfit.resize(0);
		mll = Transfer(unused_pfit, pmat, sdit, err);
		if (err != "")
			return;
		// calculate the min-log-likelihood for this parameter combination
		// Note: the MLL is not saved in the MAT file, but it is printed on
		// screen. When using a saved sample, the MLL cannot be reported and is
		// given as INF. I don't think that is an issue at the moment.

		return; // we can leave this function now and return to <calc_optim>
	}

	// BLOCK 1.4. Put <pmat> on log-scale where needed, and create a new matrix
	// with bounds that are log-transformed where needed.
	for (int p = 0; p < ind_log.size(); p++)
		pmat.Value[ind_log[p]] = log10(pmat.Value[ind_log[p]]);
	// put parameters that need to be on log scale on log10 scale
	// Note: from now on, first column of <pmat> will be log-transformed where
	// needed, until the end of this function. This is needed as <transfer>
	// expects that also the fixed parameters will be on log-scale when they are
	// marked as log parameters.
	bnds_tmp = pmat; // copy parameter bounds to a temporary version
	for (int p = 0; p < ind_log.size(); p++)
	{
		bnds_tmp.Min[ind_log[p]] = log10(bnds_tmp.Min[ind_log[p]]);
		// and log10-transform these bounds where needed
		bnds_tmp.Max[ind_log[p]] = log10(bnds_tmp.Max[ind_log[p]]);
	}
	// We want to keep the bounds in <pmat> itself on the normal scale. Reason
	// is mainly that we'll save <pmat> at the end of this function and want to
	// avoid rounding errors in the ranges from transforming back and forth.



	//// BLOCK 2. Settings for the parameter space algorithm
	// Many of the settings are globals, defined in <initial_setup>.

	// Make options for rough simplex optimisations (based on number of fitted parameters) that will be done after several rounds
	// Note: these rough options will use a smaller total number of function
	// evaluations: quick and dirty.

	// Options for parameter optimisation. Note that many of these settings are
	// vectors: they change with each subsequent round of the analysis.
	// crit_add = SETTINGS_OPTIM.crit_add; // extra on top of chi2-criterion to select ok values for next round
	// n_tr     = SETTINGS_OPTIM.n_tr; // number of random parameter tries in round 2 and further, for each ok parameter set
	// f_d      = SETTINGS_OPTIM.f_d;  // maximum step for random mutations, as factor of initial grid spacing
	// n_ok     = SETTINGS_OPTIM.n_ok; // number of values that will at least continue to next round
	// n_conf   = SETTINGS_OPTIM.n_conf_all(n_fit,:); // stop criterion: minimum number of values within the total joint CI and inner rim, based on <n_fit>
	vector<int>tries_1;
	// vector with number of tries per parameter for the initial grid
	tries_1.resize(0);
	for (int i = 0; i < gutsPARAMETER_NO; i++)
		tries_1.push_back(optimTRIES_NO[i]);

	// We work here with the log-likelihood itself, so the chi2 criterion needs to be divided by 2.
	double chicrit_joint = 0.5 * gutsCRIT_TABLE[n_fit - 1];	// criterion for joint 95% CI or parameters
	double chicrit_single = 0.5 * gutsCRIT_TABLE[0]; // criterion for single-parameter CIs



	vector<double>chicrit_rnd;
	chicrit_rnd.resize(0);
	for (int i = 0; i < optimCRIT_ADD_NO; i++)
		chicrit_rnd.push_back(optimCRIT_ADD[i] + chicrit_joint);
	// vector with criteria for selecting new tries in each subsequent round
	double chicrit_max = Max(1.2 * chicrit_joint, chicrit_single + 1.5);



	// criterion for, roughly, a 97.5// joint CI for pruning the sample ...
	// Note: for 1 or 2 fitted parameters, we need a bit more than 1.2 times the
	// joint criterion, to make sure the profiles look nice and resampling is
	// not triggered unnecessarily. Therefore the <chicrit_single+1.5> as minimum.

	// Make sure these are defined, as we might end this function prematurely
	// when hitting slow kinetics.
	// stats      = -1; // some statistics of the run
	// pmat_print = -1; // this matrix collects best value and CIs

	//// BLOCK 3. Round 1 is using a regular grid over parameter space
	// The first round is special and different from the later rounds. Create
	// vectors with values to try for each parameter as a regular-spaced range
	// between the min and max bounds (for log-scale parameters, the range is
	// thus log-linear).
	//
	// Here, a cell array is used (<p_try>) as the number of trials differs
	// between parameters. This array is turned into a regular matrix with all
	// permutations of parameter values.

	// BLOCK 3.1. Initialisation.
	int n_rnd = 0; // counter for rounds of optimisation
	vector<vector<double> >p_try;
	// initialise <p_try> as empty cell array (one cell for each parameter)
	p_try.resize(gutsPARAMETER_NO);
	for (int p = 0; p < gutsPARAMETER_NO; p++)
		p_try[p].resize(0);
	vector<double>d_grid;
	// initialise the grid spacing vector with NaNs (will collect spacing for each parameter)
	d_grid.resize(0);

	// BLOCK 3.2. Create parameter vectors for each fitted parameter, with a regular grid.
	for (int p = 0; p < gutsPARAMETER_NO; p++) // run through parameters
	{
		if (pmat.Fit[p] == gutsFIT_PARAM)
			// if we are fitting parameter <i_p> ...
		{
			double a = bnds_tmp.Min[p];
			double b = bnds_tmp.Max[p];
			double n = (double)tries_1[p];
			double spacing = (b - a) / (n - 1.0);
			for (int m = 0; m < n; m++)
				p_try[p].push_back(a + m*spacing);
			// vector: first tries as linear range between min-max bounds
			d_grid.push_back(spacing);



			// difference between parameter values for parameter <i_p> (grid spacing)
		}
		else // if we are not fitting this parameter ...
		{
			p_try[p].push_back(pmat.Value[p]); // only try the entered value
			tries_1[p] = 1; // set number of tries to 1 for this parameter
		}
	}

	// BLOCK 3.3. Create a large matrix <coll_all> with all permutations of the
	// parameter values in <p_try>.
	int n_tries = 1;
	for (int i = 0; i < tries_1.size(); i++)
		n_tries *= tries_1[i];
	// total number of tries in this round (all permutations)



	coll_all.resize(n_tries);
	for (int i = 0; i < coll_all.size(); i++)
	{
		coll_all[i].Likelihood = HIGH_POS_DOUBLE;
		coll_all[i].Parameter.resize(gutsPARAMETER_NO);
	}
	// coll_all = 1./zeros(n_tries,n_par+1); // initialise matrix to catch all tries and their minloglik with INF
	int cntr = 0; // simple counter for where to put new values in <p_try>
	// Filling <p_try> is done with a series of nested <for> loops; there must
	// be smarter ways to get all permutations ... (but note that the lengths of
	// the vectors in <p_try> can be different).
	for (int i_1 = 0; i_1 < p_try[gutsKD].size(); i_1++)
		for (int i_2 = 0; i_2 < p_try[gutsMW].size(); i_2++)
			for (int i_3 = 0; i_3 < p_try[gutsHB].size(); i_3++)
				for (int i_4 = 0; i_4 < p_try[gutsBW].size(); i_4++)
					for (int i_5 = 0; i_5 < p_try[gutsFS].size(); i_5++)
					{
						// Collect this parameter combination in <coll_all> (last column will collect the minloglik).
						coll_all[cntr].Parameter[gutsKD] = p_try[gutsKD][i_1];
						coll_all[cntr].Parameter[gutsMW] = p_try[gutsMW][i_2];
						coll_all[cntr].Parameter[gutsHB] = p_try[gutsHB][i_3];
						coll_all[cntr].Parameter[gutsBW] = p_try[gutsBW][i_4];
						coll_all[cntr].Parameter[gutsFS] = p_try[gutsFS][i_5];
						cntr++; // increase counter by 1
					}

	// BLOCK 3.4. Run through all elements of <coll_all> and calculate their
	// likelihood. This could be integrated into the previous series of <for>
	// loops ...

	// check for abortion
	Application->ProcessMessages();
	if (AbortCalibration)
		return;

	as = "Starting round " + AnsiString(n_rnd + 1) + " with initial grid of " + AnsiString(n_tries) + " parameter sets";
	memo->Lines->Add(as);
	for (int t = 0; t < n_tries; t++) // run through all elements of <coll_all> and collect their likelihood
	{
		pfit.resize(0);
		for (int p = 0; p < ind_fit.size(); p++)
			pfit.push_back(coll_all[t].Parameter[ind_fit[p]]);
		// next, try this set of parameter values
		coll_all[t].Likelihood = Transfer(pfit, pmat, sdit, err);
		if (err != "")
			return;
		// calculate the min-log-likelihood for this parameter combination,
		// and collect it in the last column of <coll_all>
	}

	// BLOCK 3.5. Extract some useful matrices from the total <coll_all> matrix.
	// Decide which sets to continue with in the next round. These will go into
	// the new matrix <coll_ok>.
	for (int i = 0; i < coll_all.size(); i++)
		if (coll_all[i].Likelihood > HIGH_POS_DOUBLE - 1)
			// remove the ones that have INF as minloglik
		{
			coll_all[i] = coll_all[coll_all.size() - 1];
			coll_all.resize(coll_all.size() - 1);
			i--;
		}
	SortParameterSpaceList(coll_all);

	if (coll_all.size() < 1)
	{
		ShowMessage("Error: Parameter space contains no sample!");
		return;
	}

	mll = coll_all[0].Likelihood;
	// lowest MLL; <coll_all> is sorted, so first is the best fitting one so far
	int ind_cont;
	for (int k = coll_all.size() - 1; k >= 0; k--)
		if (coll_all[k].Likelihood < mll + chicrit_rnd[0])
			// index to last MLL that is within the criterium to continue with
		{
			ind_cont = k;
			break;
		}

	ind_cont = Max(ind_cont, optimN_OK - 1);
	// take at least the <n_ok> best ones ...
	ind_cont = Min(ind_cont, coll_all.size() - 1);
	// <n_ok> should always be smaller than the length of <coll_all>
	// This latter check is only relevant when we fit two (or one?) parameters,
	// otherwise the number of initial tries will always exceed <n_ok>.

	// Check if we found a lot of ok values (that can for example happen when
	// the ranges are set much tighter by the user).
	int ind_cont2;
	if (ind_cont + 1 > 1 * optimN_CONF_ALL[n_fit - 1][0])
		// if we already have more than what we finally need ...
	{
		for (unsigned int k = 0; k < coll_all.size(); k++)
			if (coll_all[k].Likelihood - mll > chicrit_rnd[1])
				// how many within *next* chi2 criterion?
			{
				ind_cont2 = k;
				break;
			}
		ind_cont = Max(optimN_CONF_ALL[n_fit - 1][0] - 1, ind_cont2);
		// take highest from end number or the ones within the next chi-square criterion
		// This ensures that rather bad values (within <chicrit_rndi(2)>) are
		// still included at this point. We should take care not to remove too
		// many values-to-try early in the run.
	}



	coll_ok.resize(0);
	for (int i = 0; i < ind_cont + 1; i++)
		coll_ok.push_back(coll_all[i]);
	// take the <ind_cont> best ones to continue with

	// BLOCK 3.6. Display status on screen, make plot, and prepare settings for next round.
	as = "  Status: best fit so far is (minloglik) " + AnsiString(mll);
	memo->Lines->Add(as);
	// And make a plot of the progress so far (one plot that will be updated after each round)
	// PLOTTING IS DONE ELSEWHERE

	// Set all settings for the next round of optimisation.
	n_rnd++; // increase counter for rounds by 1
	int n_tr_i = optimN_TR[n_rnd];	// number of random parameter tries in round 2



	double f_d_i = optimF_D[n_rnd];	// maximum step as factor of grid spacing for random search



	double chicrit_i = chicrit_rnd[n_rnd]; // chi2 criterion to select ok values



	// Also check after first round if it's not too many. If we have a lot of
	// parameter sets to continue with, we can use less tries in the next round.
	if (ind_cont + 1 > 0.5 * optimN_CONF_ALL[n_fit - 1][0])
		// if we have more than half of what we finally need ...
	{
		n_tr_i = 0.5 * n_tr_i;
		// decrease the number of tries per set for next round
		if (ind_cont + 1 >= 1 * optimN_CONF_ALL[n_fit - 1][0])
			// if we have more than what we finally need in total ...
				n_tr_i = 0.5 * n_tr_i;
		// AGAIN decrease the number of tries per set
	}



	if (ind_cont > -1)
		n_tr_i = Min(n_tr_i, Max(2, (int)(10 * optimN_CONF_ALL[n_fit - 1][0] / (ind_cont + 1))));
	// hard limit for the number of tries per set for next round
	// This allows a max of 10x the target value to be tried in the next round
	// (scaling back <n_tr_i>, with a minimum of 2). These checks should ensure
	// that we don't have a huge amount of sets to try in round 2.



	// Prune <coll_all> to remove all values that are outside highest chi2 criterion.
	for (int i = 0; i < coll_all.size(); i++)
		if (coll_all[i].Likelihood >= mll + chicrit_max)
		{
			coll_all.resize(i);
			break;
		}



	//// BLOCK 4. Subsequent rounds are automated in a while loop

	int flag_stop = 0;
	// flag for when we can stop the analysis (sufficient points found: 1)
	int flag_inner = 0; // flag for when we will focus on inner rim (1)
	int ind_final, ind_single, ind_cont_t, ind_cont_a, ind_inner;

	vector<TParameterSpace>coll_tries;



	while (flag_stop != 1) // continue until this flag is set to 1
	{
		// check for abortion
		Application->ProcessMessages();
		if (AbortCalibration)
			return;

		as = "Starting round " + AnsiString(n_rnd + 1) + ", refining a selection of " + AnsiString(coll_ok.size()) +
			" parameter sets, with " + AnsiString(n_tr_i) + " tries each";
		memo->Lines->Add(as);

		// BLOCK 4.1. Call <rand_mutations> to mutate <coll_ok>, give each new set
		// an MLL, and return it in <coll_tries>.
		vector<double>d_grid_tmp = d_grid;

		for (int i = 0; i < d_grid_tmp.size(); i++)
			d_grid_tmp[i] *= f_d_i;

		coll_tries = Rand_Mutations(pmat, coll_ok, sdit, bnds_tmp, n_tr_i, d_grid_tmp, err);
		if (err != "")
			return;

		for (unsigned int i = 0; i < coll_tries.size(); i++)
			coll_all.push_back(coll_tries[i]); // add the tries to the total <coll_all>

		SortParameterSpaceList(coll_all);
		// sort the combined set based on minloglik

		// BLOCK 4.2. Do an optimisation here, with low precision, to improve
		// the best value so far. Add the optimised best set to <coll_all>.

        if (coll_all.size() < 1)
		{
			ShowMessage("Error: Parameter space contains no sample!");
			return;
		}
		pfit.resize(0);
		for (unsigned int p = 0; p < ind_fit.size(); p++)
			pfit.push_back(coll_all[0].Parameter[ind_fit[p]]);
		// copy best values to <pfit> (<coll_all> is sorted, so first is the best fitting one so far)
		// Only parameter values that are to be fitted; use as starting values for <fminsearch>.

		Setup_Transfer(pfit, pmat, sdit, CalibData_All);
		ERR = "";
		nelmin(Transfer_External, NelminN, NelminStart, NelminXmin, &NelminYnewlo, nelminROUGH_REQMIN, NelminStep, nelminKONVGE,
			optimROUGH_MAXFUNEVALS_PER_FITPARAM*NelminN, &NelminIcount, &NelminNumres, &NelminIfault);
		if (ERR != "")
			return;
		phat.resize(0);
		for (int p = 0; p < NelminN; p++)
			phat.push_back(NelminXmin[p]);
		mll = NelminYnewlo;



		coll_all.insert(coll_all.begin(), coll_all[0]);
		// copy the previous best to the first position
		for (unsigned int p = 0; p < ind_fit.size(); p++)
			coll_all[0].Parameter[ind_fit[p]] = phat[p];
		// update the best fitting one with the new fitted parameters and MLL
		coll_all[0].Likelihood = mll;
		// Note: this is a bit awkward but ensures that also the fixed values
		// are correctly copied (<phat> only contains the fitted parameters).

		// BLOCK 4.3. Derive some useful indices from <coll_all> and
		// <coll_tries>. Find the last value in <coll_all> and <coll_tries> that
		// still is within a certain criterion.
		for (int k = coll_all.size() - 1; k >= 0; k--)
			if (coll_all[k].Likelihood < mll + chicrit_joint)
				// index in <coll_all> for total joint CI
			{
				ind_final = k;
				break;
			}
		for (int k = coll_all.size() - 1; k >= 0; k--)
			if (coll_all[k].Likelihood < mll + chicrit_single)
				// index in <coll_all> for inner rim
			{
				ind_single = k;
				break;
			}
		for (int k = coll_tries.size() - 1; k >= 0; k--)
			if (coll_tries[k].Likelihood < mll + chicrit_i)
				// index in just-tried sets that qualify for continuation to next round
			{
				ind_cont_t = k;
				break;
			}
		for (int k = coll_all.size() - 1; k >= 0; k--)
			if (coll_all[k].Likelihood < mll + chicrit_i)
				// index in <coll_all> that qualify for continuation to next round
			{
				ind_cont_a = k;
				break;
			}
		for (int k = coll_all.size() - 1; k >= 0; k--)
			if (coll_all[k].Likelihood < mll + chicrit_single + 0.2)
				// index for inner rim, with a little extra
			{
				ind_inner = k;
				break;
			}

		// Two additional indices (roughly for a 97.5// joint CI) in case we run
		// into trouble (if we try too many new parameters with chicrit_i).
		// These are indices to the last entry in the two matrices that still
		// are within the total cloud that we like to calculate (slightly more
		// than <chicrit_joint>).
		int ind_cont_t2, ind_cont_a2;
		for (int k = coll_tries.size() - 1; k >= 0; k--)
			if (coll_tries[k].Likelihood < mll + chicrit_max)
			{
				ind_cont_t2 = k;
				break;
			}
		for (int k = coll_all.size() - 1; k >= 0; k--)
			if (coll_all[k].Likelihood < mll + chicrit_max)
				// index for inner rim, with a little extra
			{
				ind_cont_a2 = k;
				break;
			}



		as = "  Status: " + AnsiString(ind_final + 1) + " sets within total CI and " + AnsiString(ind_single + 1) +
			" within inner. Best fit: " + AnsiString(coll_all[0].Likelihood);
		memo->Lines->Add(as);

		// BLOCK 4.4. Try to catch slow kinetics at this point: if there are
		// signs, redo the first round with <mw> on log-scale! It tests on a
		// part of <coll_all>: only the sets that are within the joint 95% CI
		// (with a minimum of <n_ok> sets).
		if (pmat.Fit[gutsKD] == gutsFIT_PARAM &&
			pmat.Fit[gutsMW] == gutsFIT_PARAM &&
			pmat.Scale[gutsMW] == gutsNORM_SCALE_PARAM &&
			pmat.Scale[gutsKD] == gutsLOG_SCALE_PARAM &&
			bnds_tmp.Max[gutsMW] / bnds_tmp.Min[gutsMW] > 10)
		{
			// Only perform this check when both <kd> and <mw> are fitted, <mw>
			// is still on normal scale, and <kd> is still on log-scale. If one
			// of these things is different, the user has tampered with the
			// parameter ranges and he/she is on his/her own! And, also don't do
			// this check when the range of <mw> is less than a factor of 10
			// (when the user has modified the range); for a small range, normal
			// scale will generally suffice.

			coll_tst.resize(0);
			for (int i = 0; i < Max(ind_final + 1, optimN_OK); i++)
				coll_tst.push_back(coll_all[i]);
			// parameter set used to test for slow kinetics
			flag_restart = 0; // don't flag for a restart unless we need to

			double min_mw = HIGH_POS_DOUBLE;
			double min_kd = HIGH_POS_DOUBLE;
			for (int i = 0; i < coll_tst.size(); i++)
			{
				if (coll_tst[i].Parameter[gutsMW] < min_mw)
					min_mw = coll_tst[i].Parameter[gutsMW];
				// minimum (non-zero) value of <mw> in the test matrix
				if (coll_tst[i].Parameter[gutsKD] < min_kd)
					min_kd = coll_tst[i].Parameter[gutsKD];
				// minimum value of <kd> in the test matrix
			}

			double check_corr;
			vector<double>corr_mw, corr_kd;
			corr_mw.resize(0);
			corr_kd.resize(0);
			for (int i = 0; i < coll_tst.size(); i++)
			{
				corr_mw.push_back(log10(coll_tst[i].Parameter[gutsMW]));
				corr_kd.push_back(coll_tst[i].Parameter[gutsKD]);
			}
			check_corr = CalcCorrCoef(corr_mw, corr_kd);
			// correlation between <mw> and <kd> on log scale
			// For slow kinetics, <mw> and <kd> will extend to the lower part of
			// their range. This might fail when users modify the ranges.
			// However, then we still have the check on the correlation
			// coefficient.
			double crit_mw = (min_mw - bnds_tmp.Min[gutsMW]) / (bnds_tmp.Max[gutsMW] - bnds_tmp.Min[gutsMW]);
			// distance from lower bound as fraction of range
			double crit_kd = (min_kd - bnds_tmp.Min[gutsKD]) / (bnds_tmp.Max[gutsKD] - bnds_tmp.Min[gutsKD]);
			// distance from lower bound as fraction of range



			// The check is now pretty strict ... too strict? Want to avoid
			// putting <mw> on log scale when we run into single-dose runaway ...



			if (check_corr > optimSLOWKIN_CORR)
				// is there a sufficiently strong positive correlation between <mw> and <kd>?
				if (crit_mw < optimSLOWKIN_PARS || crit_kd < optimSLOWKIN_PARS)
				{
					// And is either <kd> or <mw> at lower part of their bounds?
					flag_restart = 1; // then flag for a restart



					// close(figh) // close the figure we have been building

					// Calculate min and max, of the parameter cloud we tested, for
					// each parameter (this creates a matrix). This will be used to
					// create new (smaller) starting ranges for a new round.
					vector<double>minmax_min_tmp;
					vector<double>minmax_max_tmp;
					minmax_min_tmp.resize(gutsPARAMETER_NO);
					minmax_max_tmp.resize(gutsPARAMETER_NO);
					for (int p = 0; p < gutsPARAMETER_NO; p++)
					{
						minmax_min_tmp[p] = HIGH_POS_DOUBLE;
						minmax_max_tmp[p] = HIGH_NEG_DOUBLE;
					}

					for (int i = 0; i < coll_tst.size(); i++)
						for (int p = 0; p < gutsPARAMETER_NO; p++)
						{
							if (coll_tst[i].Parameter[p] < minmax_min_tmp[p])
								minmax_min_tmp[p] = coll_tst[i].Parameter[p];
							if (coll_tst[i].Parameter[p] > minmax_max_tmp[p])
								minmax_max_tmp[p] = coll_tst[i].Parameter[p];
						}
					for (int p = 0; p < ind_log.size(); p++) // and put on normal scale where needed
					{
						if (minmax_min_tmp[ind_log[p]] < log10(HIGH_POS_DOUBLE))
							minmax_min_tmp[ind_log[p]] = pow(10.0, minmax_min_tmp[ind_log[p]]);
						else
							minmax_min_tmp[ind_log[p]] = HIGH_POS_DOUBLE;
						if (minmax_max_tmp[ind_log[p]] < log10(HIGH_POS_DOUBLE))
							minmax_max_tmp[ind_log[p]] = pow(10.0, minmax_max_tmp[ind_log[p]]);
						else
							minmax_max_tmp[ind_log[p]] = HIGH_POS_DOUBLE;
					}
					minmax.Min = minmax_min_tmp;
					minmax.Max = minmax_max_tmp;
					minmax.Value[0] = 1;
					// information criterium for calc_optim to do a restart

					return; // and go back to <calc_optim> to force a restart
				}
		}

		// BLOCK 4.5. This is the tricky bit! This block is aiming to make sure
		// not to try too few or too many points in the next round, but select
		// an efficient set for continuation in <coll_ok>.
		//
		// Note: the settings here are all tweaked to get good results in the
		// cases tested. These setting could be made part of the global setting
		// <SETTINGS_OPTIM>. However, I feared that that would make the code
		// unreadable (these settings only make sense in their context), and is
		// not really necessary. However, for full flexibility, it can be
		// considered to put them in the global.



		if (ind_final + 1 >= optimN_CONF_ALL[n_fit - 1][0])
			// do we already have enough parameter sets in the total joint CI?
		{



			if (ind_single + 1 >= optimN_CONF_ALL[n_fit - 1][1])
				// and also enough in the inner rim?
					flag_stop = 1; // then we can stop!
			else // what to do if there are enough within the outer rim, but few in the inner?
			{
				memo->Lines->Add("  Next round will focus on inner rim (outer rim has enough points)");
				coll_ok.resize(0);
				for (int i = 0; i < ind_inner + 1; i++)
					coll_ok.push_back(coll_all[i]);
				// only take the ones that are within the inner rim (plus a little extra)
				flag_inner = 1;
				// signal that we'll do inner rim only in next round



				if (ind_inner + 1 < optimN_OK)
					// if there are very few currently in the inner rim, take the <n_ok> best ones from <coll_all> ...
				{
					coll_ok.resize(0);
					for (int i = 0; i < optimN_OK; i++)
						coll_ok.push_back(coll_all[i]);
					// take the best <n_ok> values to continue to next round
				}
				else if (ind_inner + 1 > 0.5 * optimN_CONF_ALL[n_fit - 1][1])
					// if we already have quite some values in inner rim ...
				{
					// Focus on the <n_ok> sets close to the rim (now, <n_ok> on both sides of the cut-off)
					coll_ok.resize(0);
					for (int i = max(0, ind_single - optimN_OK); i < ind_single + 1 + optimN_OK; i++)
						coll_ok.push_back(coll_all[i]);
					coll_ok.push_back(coll_all[0]);
					// also add the optimised best value in there ... (helps to look for better optimum)
				}
				else
				{
					// do nothing
				}



			}
		}
		else // then there are not enough accepted parameter sets in the total joint CI
		{



			if (ind_cont_t + 1 > optimN_OK)
				// if there are enough in <coll_tries> to continue with ...
			{
				// in principle, the next round will continue from sets in
				// <coll_tries> and not from <coll_all>. The reason is that it is
				// better to propagate new sets than to continue propagating
				// sets that have been propagated before already.



				if (ind_cont_t + 1 > 2.0 * optimN_CONF_ALL[n_fit - 1][0])
					// if we will try more than 2 times of what we finally need ...
				{



					if (ind_final + 1 > 0.5 * optimN_CONF_ALL[n_fit - 1][0])
						// if we already have half the values we need
					{
						// focus on values around the edge of the inner rim as
						// that is where we need most precision.
						coll_ok.resize(0);
						for (int i = Max(0, ind_single - optimN_OK); i < min(ind_cont_t + 1, ind_single + 1 + optimN_OK); i++)
							coll_ok.push_back(coll_tries[i]);
					}
					else // then we have a lot of values to try, but not so much accepted yet
					{
						if (coll_tries[2 * optimN_CONF_ALL[n_fit - 1][0]].Likelihood > mll + chicrit_max)
							// at 2 * <n_conf(1)>, do we still include some bad points?
								ind_cont_t = 2 * optimN_CONF_ALL[n_fit - 1][0] - 1;
						// limit continuation points to the best 2 * <n_conf(1)>
						else // then it is not a good idea to limit continuation to 2 * <n_conf(1)>
								ind_cont_t = ind_cont_t2;
						// take the sets within <chicrit_max>
						// This could still be a lot ... but then we'll
						// trigger a reduction in new mutations.
						coll_ok.resize(0);
						for (int i = 0; i < ind_cont_t + 1; i++)
							coll_ok.push_back(coll_tries[i]);



					}
				}
				else // then we don't try too much in the next round, so just use <ind_cont_t> as planned
				{
					coll_ok.resize(0);
					for (int i = 0; i < ind_cont_t + 1; i++)
						coll_ok.push_back(coll_tries[i]);
				}
				coll_ok.push_back(coll_all[0]);
				// also add the optimised best value in there ... (helps to look for better optimum)
			}
			else // there are NOT enough values in <coll_tries> to continu with ... then we need to look at <coll_all>.
			{



				if (ind_cont_a + 1 > optimN_OK)
					// if there are enough reasonable sets in total <coll_all>
				{



					if (ind_cont_a + 1 > 2 * optimN_CONF_ALL[n_fit - 1][0])
						// if we will try more than two times of what we finally need ...
					{



						if (coll_all[2 * optimN_CONF_ALL[n_fit - 1][0]].Likelihood > mll + chicrit_max)
							// at 2 * <n_conf(1)>, do we still include some bad points?
								ind_cont_a = 2 * optimN_CONF_ALL[n_fit - 1][0] - 1;
						// limit to best 2 * <n_conf(1)> to continue
						else
							ind_cont_a = ind_cont_a2;
						// otherwise take the ones within <crit_max>
						// this could still be a lot ...



					}
					coll_ok.resize(0);
					for (int i = 0; i < ind_cont_a + 1; i++)
						coll_ok.push_back(coll_all[i]);
					// continue with selection from the total <coll_all>
				}
				else // if all else fails ...
				{
					coll_ok.resize(0);
					for (int i = 0; i < optimN_OK; i++)
						coll_ok.push_back(coll_all[i]);
					// take the best <n_ok> values from the total <coll_all>
				}



			}
		}

		// Now that we know the set that we will continue with, it is time to
		// update the plot of parameter space. And make a plot of the progress
		// so far
        // PLOTTING IS DONE ELSEWHERE

		// BLOCK 4.6. See if we need a new round, and if so, prepare for it.
		// Again, special care is taken to avoid taking too few or too many new
		// tries in the next round. Therefore, the tricky part is to come up
		// with an efficient value for <n_tr_i> (number of random mutations per
		// set in the next round).
		//
		// Note: the settings here are all tweaked to get good results in the
		// cases tested. These setting could be made part of the global setting
		// <SETTINGS_OPTIM>. However, I feared that that would make the code
		// unreadable (these settings only make sense in their context), and is
		// not really necessary. However, for full flexibility, it can be
		// considered to put them in the global.

		if (n_rnd == 12 - 1 && flag_stop == 0)
			// at some point, we need to force a stop ... and examine what went wrong
		{
			flag_stop = 1; // let's stop here
			as = "We have now done " + AnsiString(n_rnd + 1) +
				" rounds without reaching the stopping criterion ... we stop here!";
			memo->Lines->Add(as);
		}

		// If we need another round, prepare for it.
		if (flag_stop != 1)
		{
			n_rnd++; // increase counter for rounds by 1



			if (n_rnd < optimN_TR_NO)
				// do we still have values for the optim. criteria left in our options set?
			{
				n_tr_i = optimN_TR[n_rnd]; // number of mutations per set
				f_d_i = optimF_D[n_rnd];
				// maximum step as factor of grid spacing for mutations
				chicrit_i = chicrit_rnd[n_rnd];
				// criterion to select ok values for continuation
			}
			else // otherwise, take the last values for any additional round
			{
				n_tr_i = optimN_TR[optimN_TR_NO - 1];
				f_d_i = optimF_D[optimF_D_NO - 1];
				chicrit_i = chicrit_rnd[chicrit_rnd.size()];
			}



			// Modify number of tries if we have a lot or only few sets within the total cloud or inner rim.
			vector<double>crit_ntry;
			// vector: how far are we from target number of sets in total and inner rim?
			crit_ntry.resize(0);
			crit_ntry.push_back((double)(ind_final + 1) / optimN_CONF_ALL[n_fit - 1][0]);
			crit_ntry.push_back((double)(ind_single + 1) / optimN_CONF_ALL[n_fit - 1][1]);



			if (crit_ntry[flag_inner] > 0.75 || coll_ok.size() > 2000)
			{
				// If we already have more than 75// of what we finally need, or more than 2000 sets to continue with
				// Note: when <flag_inner> = 1, we look at the status of the inner rim!
				n_tr_i = n_tr_i / 2;
				// decrease the number of tries per set for next round
			}

			else if (n_rnd > 3 - 1)
				// starting at round 4, we start to worry if we haven't found so many yet
			{
				if (coll_ok.size() < 0.5 * optimN_CONF_ALL[n_fit - 1][flag_inner] && coll_ok.size() < 1000)
				{
					// If we have less than 50// of what we finally need, and less than 1000 sets to try next ...
					n_tr_i *= 2; // use twice as many tries in the next round
					if (n_rnd > 4 - 1 && crit_ntry[flag_inner] < 0.25)
						// if there is very few when we start round 5 ...
							n_tr_i *= 2;
					// use twice as many tries (again) in the next round
				}
				else if (n_rnd > 7 - 1)
					// in the last few rounds, always worry if we still have less than 75// of the final number ...
						n_tr_i *= 2;
				// use twice as many tries in the next round
				else
				{
					// do nothing
				}
			}
			n_tr_i = Min(n_tr_i, Max(2, 10 * optimN_CONF_ALL[n_fit - 1][flag_inner] / coll_ok.size()));
			// limit the number of tries per set for next round
			// This allows a max of 10x the target value to be tried in the next
			// round (scaling back <n_tr_i>). Well, sometimes more, as at least
			// 2 tries will be allowed for each set in <coll_ok>. I used to have
			// a check for whether we are in the final rounds or if we found
			// very little, but I don't think that is needed (and sometimes
			// leads to large numbers of new tries). If all fails, the extra
			// sampling rounds will come to the rescue.



		}

		// Prune <coll_all> to remove all values that are outside highest criterion
		vector<TParameterSpace>coll_tmp;
		coll_tmp.resize(0);
		for (int i = 0; i < coll_all.size(); i++)
			if (coll_all[i].Likelihood < mll + chicrit_max)
				coll_tmp.push_back(coll_all[i]);
		coll_all = coll_tmp;


	}

	// BLOCK 4.7. We only get here when the while loop is finished. To improve
	// upon the best fit ... do a thorough optimisation

	memo->Lines->Add("  Finished sampling, running a simplex optimisation ...");

	if (coll_all.size() < 1)
	{
		ShowMessage("Error: Parameter space contains no sample!");
		return;
	}
	pmat.Value = coll_all[0].Parameter;
	// update <pmat> with the best value at this point
	pfit.resize(0);
	for (unsigned int p = 0; p < ind_fit.size(); p++)
		pfit.push_back(pmat.Value[ind_fit[p]]);
	// copy best values to <pfit>, for parameters that are to be fitted

	Setup_Transfer(pfit, pmat, sdit, CalibData_All);
	ERR = "";
	nelmin(Transfer_External, NelminN, NelminStart, NelminXmin, &NelminYnewlo, nelminSTD_REQMIN, NelminStep, nelminKONVGE,
		optimSTD_MAXFUNEVALS_PER_FITPARAM*NelminN, &NelminIcount, &NelminNumres, &NelminIfault);
	if (ERR != "")
		return;
	phat.resize(0);
	for (int p = 0; p < NelminN; p++)
		phat.push_back(NelminXmin[p]);
	mll = NelminYnewlo;

	for (unsigned int p = 0; p < ind_fit.size(); p++)
		pmat.Value[ind_fit[p]] = phat[p];
	// update <pmat> with the best fit parameters
	TParameterSpace coll_tmp;
	coll_tmp.Parameter = pmat.Value;
	coll_tmp.Likelihood = mll;
	coll_all.insert(coll_all.begin(), coll_tmp);
	// place the new optimum at the top of <coll_all>

	// Update the location of the total and inner conf. region.
	for (int k = coll_all.size() - 1; k >= 0; k--)
		if (coll_all[k].Likelihood < mll + chicrit_joint)
		{
			ind_final = k;
			break;
		}
	for (int k = coll_all.size() - 1; k >= 0; k--)
		if (coll_all[k].Likelihood < mll + chicrit_single)
		{
			ind_single = k;
			break;
		}



	as = "  Status: " + AnsiString(ind_final + 1) + " sets within total CI and " + AnsiString(ind_single + 1) +
		" within inner. Best fit: " + AnsiString(coll_all[0].Likelihood);
	memo->Lines->Add(as);

	//// BLOCK 5. Calculation of profile likelihood.
	// From the sample, calculate a profile likelihood as a line in a plot of
	// MLL versus a single parameter. The line is the lowest value of the MLL
	// that can be reached at each value of the parameter (target parameter
	// fixed, remaining parameters fitted).

	// check for abortion
	Application->ProcessMessages();
	if (AbortCalibration)
		return;

	n_rnd++; // increase counter for rounds by 1
	as = "Starting round " + AnsiString(n_rnd + 1) + ", creating the profile likelihoods for each parameter";
	memo->Lines->Add(as);

	// Call function to do the profiling. If profiling finds a lower MLL, <phat>
	// returns the new best-fitting parameters, and <mll> the new MLL (otherwise,
	// the previous MLL is returned). <coll_prof> will be the profile info for
	// each parameter (cell array), and <coll_ok> the parameter sets that are
	// candidates for further sampling.
	Calc_Proflik(coll_prof, coll_ok, phat, mll, coll_prof_pruned, flag_short, pmat, coll_all, sdit, bnds_tmp);



	if (phat.size() > 0) // if profiling led to a better optimum
	{
		for (int p = 0; p < ind_fit.size(); p++)
			pmat.Value[ind_fit[p]] = phat[p];
		// update <pmat> with the better fitting parameters
		TParameterSpace coll_tmp;
		coll_tmp.Parameter = pmat.Value;
		coll_tmp.Likelihood = mll;
		coll_all.insert(coll_all.begin(), coll_tmp);
		// place the new optimum at the top of <coll_all>
		if (coll_all[1].Likelihood - coll_all[0].Likelihood > optimREAL_BETTER) // if there is a real improvement
				coll_ok.insert(coll_ok.begin(), coll_all[0]);
		// also add the new optimum to <coll_ok>, so trigger resampling if <coll_ok> was empty
		// Update the location of the total and inner conf. region.
		for (int k = coll_all.size() - 1; k >= 0; k--)
			if (coll_all[k].Likelihood < mll + chicrit_joint)
			{
				ind_final = k;
				break;
			}
		for (int k = coll_all.size() - 1; k >= 0; k--)
			if (coll_all[k].Likelihood < mll + chicrit_single)
			{
				ind_single = k;
				break;
			}

	}

	if (ind_final == 0)
	{
		err += "Optimisation failed, this data set requires some specialist attention. There possibly exists a very narrow global optimum. ";
		return;
    }

	// plot_grid(pmat,[],coll_all,coll_prof_pruned,figh); // update the plot of parameter space, now with profiles
	as = "  Status: " + AnsiString(ind_final + 1) + " sets within total CI and " + AnsiString(ind_single + 1) +
		" within inner. Best fit: " + AnsiString(coll_all[0].Likelihood);
	memo->Lines->Add(as);

	// // TEMP save preliminary plot, just after first profiling (for design doc)
	// figure(figh)  // use existing handle and make sure that the multiplot is the current plot
	// save_plot(figh,['output_report',filesep,[savestr,'_pre']],3) // save parspace plot as pdf in output folder

	//// BLOCK 6. Do an extra round of sampling when profiling indicates problems.
	// The profiling will identify points where there is a difference between
	// the profile and the sampling points. This generally indicates that
	// sampling was not sufficiently covering parameter space. These points are
	// returned in <coll_ok>, which we can use for additional rounds of sampling.


	if (coll_ok.size() > 0 || ind_single + 1 < optimN_CONF_ALL[n_fit - 1][1])
		// if we have candidate sets for additional sampling, or if we don't have enough in inner rim
	{
		// When profiling finds a much better optimum, we can suddenly end up
		// with insufficient sets in the inner rim (as it will shift the profile
		// up).

		// BLOCK 6.1. Initial things when we start the extra sampling.

		if (coll_ok.size() == 0 || (ind_single + 1 < optimN_CONF_ALL[n_fit - 1][1] && coll_ok.size() < 10))
			// when empty or when there are too few in inner rim (happens when we found a much better optimum in profiling)
		{
			// Add values around the edge of the (new) inner rim as that is where we need most precision.
			for (int i = Max(0, ind_single - optimN_OK); i < Min(ind_single + 1 + optimN_OK, coll_all.size()); i++) // This is now modified with a <min> statement to catch cases where <coll_all> does not have the required number of elements.
					coll_ok.push_back(coll_all[i]);
		}

		TParameterMatrix edges_cloud; // edges of the joint 95% cloud
		vector<double>edges_cloud_min_tmp;
		vector<double>edges_cloud_max_tmp;
		edges_cloud_min_tmp.resize(gutsPARAMETER_NO);
		edges_cloud_max_tmp.resize(gutsPARAMETER_NO);
		for (int p = 0; p < gutsPARAMETER_NO; p++)
		{
			edges_cloud_min_tmp[p] = HIGH_POS_DOUBLE;
			edges_cloud_max_tmp[p] = HIGH_NEG_DOUBLE;
		}
		for (int i = 0; i < ind_final + 1; i++)
			for (int p = 0; p < gutsPARAMETER_NO; p++)
			{
				if (coll_all[i].Parameter[p] < edges_cloud_min_tmp[p])
					edges_cloud_min_tmp[p] = coll_all[i].Parameter[p];
				if (coll_all[i].Parameter[p] > edges_cloud_max_tmp[p])
					edges_cloud_max_tmp[p] = coll_all[i].Parameter[p];
			}
		edges_cloud.Min = edges_cloud_min_tmp;
		edges_cloud.Max = edges_cloud_max_tmp;

		//d_grid.resize(edges_cloud.Min.size());
		d_grid.resize(0);
		for (int p=0; p<gutsPARAMETER_NO; p++)
		{
			//d_grid[i] = optimD_EXTRA * (edges_cloud.Max[i] - edges_cloud.Min[i]);
			if (pmat.Fit[p] == gutsFIT_PARAM)
			{
				d_grid.push_back(optimD_EXTRA * (edges_cloud.Max[p] - edges_cloud.Min[p]));

			}
		}
		// create a new grid spacing as fraction of the new total parameter range

		f_d_i = optimF_D_EXTRA;
		// maximum jump size as fraction of the grid spacing


		SortParameterSpaceList(coll_ok); // sort <coll_ok> based on minloglik
		for (int i = coll_ok.size() - 1; i >= 0; i--)
			if (coll_ok[i].Likelihood < mll + optimCRIT_ADD_EXTRA)
			{
				coll_ok.resize(i + 1);
				// prune <coll_ok> to remove all values that are too far outside inner rim
				break;
			}

		int n_rnd_x = 0; // set simple counter for extra rounds to zero
		int n_test = 0;
		// a simple counter for how many times we trigger new profiling


		if (ind_single + 1 < optimN_CONF_ALL[n_fit - 1][1])
			memo->Lines->Add
				("  Profiling has led to a new optimum, which left insufficient sets in the innter rim: extra sampling rounds will be started."
				);
		else
			memo->Lines->Add("  Profiling has detected gaps between profile and sample, which requires extra sampling rounds.");

		// BLOCK 6.2. Continue sampling and testing until there is nothing left
		// to worry about (and <coll_ok> will be empty).

		while (coll_ok.size() > 0)
			// keep going until we are not given new sets to mutate
		{
			// BLOCK 6.2.1. Prepare for a new round of sampling.
			n_rnd++; // increase counter for rounds by 1
			n_rnd_x++; // increase counter for extra rounds by 1

			// Select number of mutations per parameter set in <coll_ok>.
			n_tr_i = 400; // basic number of new tries per set
			n_tr_i = Min(n_tr_i, Max(2, 5 * optimN_CONF_ALL[n_fit - 1][0] / coll_ok.size()));
			// make sure that we don't have more than a fixed number new tries
			// This scales <n_tr_i> back if needed (avoids extreme amounts of
			// mutations) to a max. of 5 times the value we aim for in the total
			// cloud (with a minimum of 2 tries per set).


			// check for abortion
			Application->ProcessMessages();
			if (AbortCalibration)
				return;

			as = "Starting round " + AnsiString(n_rnd + 1) + ", (extra " + AnsiString(n_rnd_x) + ") refining a selection of " +
				AnsiString(coll_ok.size()) + " parameter sets, with " + AnsiString(n_tr_i) + " tries each";
			memo->Lines->Add(as);

			// BLOCK 6.2.2. Call <rand_mutations> to mutate <coll_ok>, give each
			// new set an MLL, and return it in <coll_tries>.
			vector<double>d_grid_tmp = d_grid;
			for (int i = 0; i < d_grid_tmp.size(); i++)
			{
				d_grid_tmp[i] *= f_d_i;

			}
			coll_tries = Rand_Mutations(pmat, coll_ok, sdit, bnds_tmp, n_tr_i, d_grid_tmp, err);
			if (err != "")
				return;


			for (int i = 0; i < coll_tries.size(); i++)
				coll_all.push_back(coll_tries[i]);
			// add the tries to <coll_all>
			SortParameterSpaceList(coll_all); // also sort based on minloglik

			// Prune <coll_all> to remove all values that are outside highest criterion
			for (int i = 0; i < coll_all.size(); i++)
				if (coll_all[i].Likelihood >= mll + chicrit_max)
				{
					coll_all.resize(i);
					break;
				}


			// PLOTTING IS DONE ELSEWHERE

			// Update the location of the total and inner conf. region.
			for (int k = coll_all.size() - 1; k >= 0; k--)
				if (coll_all[k].Likelihood < mll + chicrit_joint)
				{
					ind_final = k;
					break;
				}
			for (int k = coll_all.size() - 1; k >= 0; k--)
				if (coll_all[k].Likelihood < mll + chicrit_single)
				{
					ind_single = k;
					break;
				}


			as = "  Status: " + AnsiString(ind_final + 1) + " sets within total CI and " + AnsiString(ind_single + 1) +
				" within inner. Best fit: " + AnsiString(coll_all[0].Likelihood);
			memo->Lines->Add(as);

			// BLOCK 6.2.3. Run a test to see whether the sample now DOES match
			// the profile likelihood. It checks where the profile is
			// considerably lower than the sample (which means that more
			// sampling is needed), and when the sample is lower than the
			// profile (which means we may need a new profile).
			Test_Proflik(flag_profile, coll_ok, pmat, coll_prof, coll_all, err);
			if (err != "")
				return;



			// BLOCK 6.2.4. Do we need another round of extra sampling? First,
			// make sure that there are enough points in the inner rim
			// (otherwise, continue sampling).

			if (ind_single + 1 < optimN_CONF_ALL[n_fit - 1][1])
				// if we don't have enough in inner rim ... simply continue sampling until we do
			{
				// Theoretically, this could lead to an infinite loop. However,
				// I don't think that there can be cases where continuous
				// sampling won't fill the inner rim.
				for (int k = coll_tries.size() - 1; k >= 0; k--)
					if (coll_tries[k].Likelihood < mll + optimCRIT_ADD_EXTRA)
					{
						ind_cont = k;
						// only continue with new tries that are close to inner rim
						break;
					}

				if (ind_cont + 1 < 0.5 * optimN_CONF_ALL[n_fit - 1][1])
					// if tries are less than half of what we finally need ...
					for (int i = 0; i < ind_cont + 1; i++)
						coll_ok.push_back(coll_tries[i]);
				// also add the <coll_ok> to it
				else
				{
					coll_ok.resize(0);
					for (int i = 0; i < ind_cont + 1; i++)
						coll_ok.push_back(coll_tries[i]);
					// otherwise, only tries
				}
				f_d_i = Max(0.1, f_d_i * 0.7);
				// decrease jumps by a factor over subsequent rounds to let extra sampling contract
			}
			else if (coll_ok.size() > 0 && n_rnd_x < 5)
			{
				// BLOCK 6.2.5. We already have enough points in the inner rim,
				// but gaps have been flagged and we haven't done more than 4
				// extra sampling rounds yet. If there are still gaps left after
				// 5 rounds, it is unlikely that more sampling will really help.

				// Continue sampling with <coll_ok>.
				f_d_i = Max(0.1, f_d_i * 0.7);
				// decrease jumps by a factor over subsequent rounds to let extra sampling contract
			}
			else if ((flag_profile[1] > 0.05 || flag_short == 1) && n_test < 2)
				// if some difference between profile and sample is spotted
			{
				// BLOCK 6.2.6. At this point, see if we have a flag for a new
				// profile. The profile flag is a vector of 2: number of points
				// flagged, and maximum distance between profile and sample. At
				// this point, I only trigger on the latter (note that I allow
				// some deviation). The inclusion of <n_test> makes sure that we
				// don't end up in an infinite loop ... if we already did 2
				// reprofiles, another one will probably not help. Also do a new
				// profiling if the previous one indicated that one or more
				// profiles were short.
				//
				// Note: there are some data sets that cannot be profiled
				// properly with the current algorithm. They continue to trigger
				// reprofiling. The consequences for the user are minor: a small
				// error on the single parameter CI and a bit longer calibration
				// time (and a sample that is a bit larger than needed).
				// Therefore, I think there is no pressing need to add more code
				// to catch these cases.

				// check for abortion
				Application->ProcessMessages();
				if (AbortCalibration)
					return;

				n_rnd++; // increase counter for rounds by 1
				as = "Starting round " + AnsiString(n_rnd + 1) + ", refining the profile likelihoods for each parameter again";
				memo->Lines->Add(as);
				n_test = n_test + 1;
				// increase the counter for number of re-profiles

				// Call function to do the profiling. If profiling finds a
				// lower MLL, <phat> returns the new best-fitting parameters
				// and <mll> the new MLL (otherwise, the previous MLL is
				// returned).
				Calc_Proflik(coll_prof, coll_ok, phat, mll, coll_prof_pruned, flag_short, pmat, coll_all, sdit, bnds_tmp);

				if (phat.size() > 0) // if profiling led to a better optimum
				{
					for (int p = 0; p < ind_fit.size(); p++)
						pmat.Value[ind_fit[p]] = phat[p];
					// update <pmat> with the better fitting parameters
					TParameterSpace coll_tmp;
					coll_tmp.Parameter = pmat.Value;
					coll_tmp.Likelihood = mll;
					coll_all.insert(coll_all.begin(), coll_tmp);
					// place the new optimum at the top of <coll_all>
					// Update the location of the total and inner conf. region.
					for (int k = coll_all.size() - 1; k >= 0; k--)
						if (coll_all[k].Likelihood < mll + chicrit_joint)
						{
							ind_final = k;
							break;
						}
					for (int k = coll_all.size() - 1; k >= 0; k--)
						if (coll_all[k].Likelihood < mll + chicrit_single)
						{
							ind_single = k;
							break;
						}
				}

				as = "  Status: " + AnsiString(ind_final + 1) + " sets within total CI and " + AnsiString(ind_single + 1) +
					" within inner. Best fit: " + AnsiString(coll_all[0].Likelihood);
				memo->Lines->Add(as);
				// PLOTTING IS DONE ELSEWHERE

				// If there is something in <coll_ok>, found by profiling, a
				// new round will be triggered. Then we need to reset the
				// extra rounds counter and jump size.
				if (coll_ok.size() > 0)
				{
					n_rnd_x = 0; // reset extra rounds counter
					f_d_i = optimF_D_EXTRA;
					// reset maximum jump size as fraction of the grid spacing
					memo->Lines->Add("  New profiling has detected gaps, which requires additional extra sampling rounds!");
				}
			}
			else
			{
				// BLOCK 6.2.7. If nothing is flagged, we can stop, but display
				// on screen if we stopped at maximum number of rounds.
				if (coll_ok.size() > 0)
				{
					as = "Exiting parameter space explorer, but still " + AnsiString(coll_ok.size()) +
						" sets flagged (too much distance between profile and sample). Check plot.";
					memo->Lines->Add(as);
				}
				if (flag_profile[1] > 0.05)
					memo->Lines->Add
						("Exiting parameter space explorer, but there still seem to be parameter sets outside of the profile curves. Check plot."
						);
				coll_ok.resize(0);
				// make <coll_ok> empty (if it is not empty already) so we can stop further sampling
			}
		}
	}

	// Prune <coll_all> to remove all values that are outside highest criterion.
	// This is needed when a (much) better optimum has been found by extra
	// sampling.
	SortParameterSpaceList(coll_all);
	for (int i = coll_all.size() - 1; i >= 0; i--)
		if (coll_all[i].Likelihood < mll + chicrit_max)
		{
			coll_all.resize(i + 1);
			break;
		}

	//// BLOCK 7. Calculate CIs for single parameters
	// Now we can extract the CIs for the single parameters. Do this separately
	// from the profiling because the profiling (and additional sampling) might
	// just find a better optimum (which shifts all profiles, and thereby the
	// CIs).

	// Start a matrix <pmat_print> that will collect the best parameters and CIs
	// (and additional info for each parameter).
	if (coll_all.size() < 1)
	{
		ShowMessage("Error: Parameter space contains no sample!");
		return;
	}
	pmat_print.BestValue = coll_all[0].Parameter;
	// add best values so far in first column
	// pmat_print(:,[2 3]) = nan; // add two columns for the single-parameter CIs


	vector<double>edges95_min_tmp;
	vector<double>edges95_max_tmp;
	edges95_min_tmp.resize(gutsPARAMETER_NO);
	edges95_max_tmp.resize(gutsPARAMETER_NO);
	for (int p = 0; p < gutsPARAMETER_NO; p++)
	{
		edges95_min_tmp[p] = HIGH_POS_DOUBLE;
		edges95_max_tmp[p] = HIGH_NEG_DOUBLE;
	}
	for (int i = 0; i < ind_final + 1; i++)
		for (int p = 0; p < gutsPARAMETER_NO; p++)
		{
			if (coll_all[i].Parameter[p] < edges95_min_tmp[p])
				edges95_min_tmp[p] = coll_all[i].Parameter[p];
			if (coll_all[i].Parameter[p] > edges95_max_tmp[p])
				edges95_max_tmp[p] = coll_all[i].Parameter[p];
		}
	pmat_print.LowerEdgeJoint95 = edges95_min_tmp;
	// add the edges of the joint 95% cloud
	pmat_print.UpperEdgeJoint95 = edges95_max_tmp;

	int parnr, ind_low, ind_hi, num_x;
	vector<TParameterSpace>parprof, prof_tst;
	double x_1, x_2, y_1, y_2, x_diff, y_diff, slope, constant;
	pmat_print.LowerCI.resize(gutsPARAMETER_NO);
	pmat_print.LowerCIWarning.resize(gutsPARAMETER_NO);
	pmat_print.UpperCI.resize(gutsPARAMETER_NO);
	pmat_print.UpperCIWarning.resize(gutsPARAMETER_NO);
	pmat_print.Broken.resize(gutsPARAMETER_NO);

	for (int p = 0; p < ind_fit.size(); p++) // run through all fitted parameters
	{
		parnr = ind_fit[p]; // index to the fitted parameter
		parprof = coll_prof[parnr];
		// read <parprof> again from the collected ones

		for (int i = 0; i < parprof.size(); i++)
			coll_all.push_back(parprof[i]);
		// add the profile information to the sample in <coll_all>!
		SortParameterSpaceList(coll_all);
		// sort <coll_all> again based on minloglik, as points have been added

		// This may help if the sampling was not very good. I do this here now,
		// as otherwise we cannot do the profiling a second time (the first
		// profile will be included in the sample, so that is always the best
		// sample point in each slice).

		// BLOCK 7.1. Check if *other* parameters are running into bounds at the
		// min/max of *this* parameter (that could have influenced the bounds).
		// This is not perfect, though it will catch the obvious cases.
		//
		// THIS BLOCK HAS BEEN REMOVED: IT DID NOT WORK WELL ENOUGH, SO WE'LL
		// LEAVE THAT FOR FUTURE VERSIONS IF NEEDED.

		// BLOCK 7.2. Find the outer bounds of the CI (we don't care for broken
		// intervals anymore; they are flagged, not calculated). The CI bounds
		// themselves are collected in <pmat_print> (column 2 and 3). Column 6
		// and 7 will get a 1 when the CI is open on the lower or upper end.
		// Column 8 will get a 1 when there are multiple zero-crossings and we
		// have a broken CI.

		prof_tst = coll_prof_pruned[parnr];
		// use the pruned version to calculate CIs  // [p] in original code, however, the pruned version contains all parameters, not only the fitted ones
		for (int i = 0; i < prof_tst.size(); i++)
			prof_tst[i].Likelihood = prof_tst[i].Likelihood - mll - chicrit_single;
		// rephrase the problem to zero finding
		// prof_tst(~isfinite(prof_tst(:,2)),:) = []; // remove any rows where the MLL is inf or nan
		// Note: INF should not happen anymore (pruned in <prune_prof>), but
		// perhaps NaNs could happen?

		// First and last value in <prof_tst> should be positive, otherwise we
		// have an open interval.
		if (prof_tst[0].Likelihood < 0)
			// that means that the CI is open on the lower end: parameter is hitting lower bound
		{
			pmat_print.LowerCI[parnr] = bnds_tmp.Min[parnr];
			// just use lower bound of parameter range
			pmat_print.LowerCIWarning[parnr] = "*";
			// and mark it in <pmat_print>
		}
		else // otherwise, we need to find the first crossing point
		{
			for (int k = 0; k < prof_tst.size(); k++)
				if (prof_tst[k].Likelihood < 0)
					// find index to first point below cut_off
				{
					ind_low = k;
					break;
				}
			// Next, linearly interpolate between points just above and below cut-off.
			x_1 = prof_tst[ind_low - 1].Likelihood;
			x_2 = prof_tst[ind_low].Likelihood;
			y_1 = prof_tst[ind_low - 1].Parameter[parnr];
			y_2 = prof_tst[ind_low].Parameter[parnr];
			x_diff = x_2 - x_1;
			y_diff = y_2 - y_1;
			if (abs(x_diff) > EPS)
			{
				slope = y_diff / x_diff;
				constant = y_1 - slope * x_1;
				pmat_print.LowerCI[parnr] = constant;
			}
			else
				pmat_print.LowerCI[parnr] = HIGH_NEG_DOUBLE;
			pmat_print.LowerCIWarning[parnr] = "";
		}
		if (prof_tst[prof_tst.size() - 1].Likelihood < 0)
			// that means that the CI is open on the upper end: parameter is hitting upper bound
		{
			pmat_print.UpperCI[parnr] = bnds_tmp.Max[parnr];
			// just use upper bound of parameter range
			pmat_print.UpperCIWarning[parnr] = "*";
			// and mark it in <pmat_print>
		}
		else
		{
			for (int k = prof_tst.size() - 1; k >= 0; k--)
				if (prof_tst[k].Likelihood < 0)
					// find index to last point below cut_off
				{
					ind_hi = k;
					break;
				}
			// Next, interpolate between points just above and below cut-off.
			x_1 = prof_tst[ind_hi].Likelihood;
			x_2 = prof_tst[ind_hi + 1].Likelihood;
			y_1 = prof_tst[ind_hi].Parameter[parnr];
			y_2 = prof_tst[ind_hi + 1].Parameter[parnr];
			x_diff = x_2 - x_1;
			y_diff = y_2 - y_1;
			if (abs(x_diff) > EPS)
			{
				slope = y_diff / x_diff;
				constant = y_1 - slope * x_1;
				pmat_print.UpperCI[parnr] = constant;
			}
			else
				pmat_print.UpperCI[parnr] = HIGH_POS_DOUBLE;
			pmat_print.UpperCIWarning[parnr] = "";
		}

		// And a final count of the number of zero crossings. When there are two
		// we have nice regular CI.
		num_x = 0;
		// this counts the number of sign changes in the profile to test
		for (int i = 1; i < prof_tst.size(); i++)
		{
			if (prof_tst[i].Likelihood > 0 && prof_tst[i - 1].Likelihood < 0)
				num_x++;
			else if (prof_tst[i].Likelihood < 0 && prof_tst[i - 1].Likelihood > 0)
				num_x++;
			else
			{
				// do nothing
			}
		}
		if (pmat_print.LowerCIWarning[parnr].length() > 0)
			// also add number of open intervals as a zero crossing
				num_x++;
		if (pmat_print.UpperCIWarning[parnr].length() > 0)
			num_x++;
		if (num_x > 2) // then we have a broken CI
				pmat_print.Broken[parnr] = true; // mark it in <pmat_print>
		else
			pmat_print.Broken[parnr] = false;
	}

	//// BLOCK 8. Final plotting saving the results to a MAT file.
	// save the sample in a MAT-file with the name of the script, with _SD or
	// _IT at the end. <load_sample> can be used to retrieve it.

	// Now we can safely put <pmat_print> on normal scale, where needed.
	for (int p = 0; p < ind_log.size(); p++) // put log parameters back on normal scale (also their CIs and total bounds)
	{
		if (pmat_print.BestValue[ind_log[p]] < log10(HIGH_POS_DOUBLE))
			pmat_print.BestValue[ind_log[p]] = pow(10.0, pmat_print.BestValue[ind_log[p]]);
		else
			pmat_print.BestValue[ind_log[p]] = HIGH_POS_DOUBLE;

		if (pmat_print.LowerCI[ind_log[p]] < log10(HIGH_POS_DOUBLE))
			pmat_print.LowerCI[ind_log[p]] = pow(10.0, pmat_print.LowerCI[ind_log[p]]);
		else
			pmat_print.LowerCI[ind_log[p]] = HIGH_POS_DOUBLE;

		if (pmat_print.UpperCI[ind_log[p]] < log10(HIGH_POS_DOUBLE))
			pmat_print.UpperCI[ind_log[p]] = pow(10.0, pmat_print.UpperCI[ind_log[p]]);
		else
			pmat_print.UpperCI[ind_log[p]] = HIGH_POS_DOUBLE;

		if (pmat_print.LowerEdgeJoint95[ind_log[p]] < log10(HIGH_POS_DOUBLE))
			pmat_print.LowerEdgeJoint95[ind_log[p]] = pow(10.0, pmat_print.LowerEdgeJoint95[ind_log[p]]);
		else
			pmat_print.LowerEdgeJoint95[ind_log[p]] = HIGH_POS_DOUBLE;

		if (pmat_print.UpperEdgeJoint95[ind_log[p]] < log10(HIGH_POS_DOUBLE))
			pmat_print.UpperEdgeJoint95[ind_log[p]] = pow(10.0, pmat_print.UpperEdgeJoint95[ind_log[p]]);
		else
			pmat_print.UpperEdgeJoint95[ind_log[p]] = HIGH_POS_DOUBLE;
	}

	// And make a plot of the final results.
	// PLOTTING IS DONE ELSEWHERE

	// Save the sample into a MAT file, so it can be used to create CIs on model
	// curves.
	for (int p = 0; p < ind_log.size(); p++) // put log-parameters to normal scale before saving
	{
		if (pmat.Value[ind_log[p]] < log10(HIGH_POS_DOUBLE))
			pmat.Value[ind_log[p]] = pow(10.0, pmat.Value[ind_log[p]]);
		else
			pmat.Value[ind_log[p]] = HIGH_POS_DOUBLE;
	}
	// save pmat in global variable
	if (sdit == gutsSD)
		PMatSD = pmat;
	else
		PMatIT = pmat;

	// Prune <coll_all> to remove all values that are outside highest criterion,
	// and only keep the columns that are fitted (<load_sample> will then be
	// able to reconstruct the entire matrix). We could decide to save only a
	// partial sample here. For example, only the values that will be propagated
	// (that makes the job of <load_sample> a bit easier.
	vector<TParameterSpace>coll_all_tmp = coll_all;
	coll_all.resize(0);
	for (int i = 0; i < coll_all_tmp.size(); i++)
		if (coll_all_tmp[i].Likelihood < mll + Max(chicrit_joint, 0.5*gutsCRIT_PROP[1]))
			coll_all.push_back(coll_all_tmp[i]);
	// in the original code, only the parameters for ind_fit and the likelihood are stored in coll_all

	// save parameter space
	if (sdit == gutsSD)
		CalibrationParameterSpaceSD = coll_all;
	else
		CalibrationParameterSpaceIT = coll_all;
	// The max function here will only come into play when we have one fitted
	// parameter (otherwise, <chicrit_joint> is highest). Without it, for one
	// fitted parameter, the propagation sample would be everything in the inner
	// rim (which equals the joint CI for df=1), and not the part within the
	// horizontal dotted lines.

	// Note that we will save the pruned version of the profile likelihood
	// <coll_prof_pruned>. At this moment, <coll_prof_pruned> is a cell array:
	// each cell is a two-column matrix with parameter values and MLLs. This can
	// be turned into a regular matrix if we like to save this information in a
	// text file (but note that <coll_prof_pruned> can be of different length
	// for different parameters).

	// SAVING IS DONE ELSEWHERE

	// Also calculate how many sets will be used for propagation.
	if (coll_all.size() < 1)
	{
		ShowMessage("Error: Parameter space contains no sample!");
		return;
	}
	int ind_prop1 = -1;
	int ind_prop2 = -1;
	for (int k = coll_all.size() - 1; k >= 0; k--)
		if (coll_all[k].Likelihood < coll_all[0].Likelihood + 0.5 * gutsCRIT_PROP[0])
		{
			ind_prop1 = k;
			break;
		}
	for (int k = coll_all.size() - 1; k >= 0; k--)
		if (coll_all[k].Likelihood < coll_all[0].Likelihood + 0.5 * gutsCRIT_PROP[1])
		{
			ind_prop2 = k;
			break;
		}
	if (ind_prop1 == -1)
		// <GLO.crit_prop(1)> could be zero (if we want entire inner rim), which leads to empty <ind_prop1>
			ind_prop1 = 0; // start from the first value

	// useful statistics for <calc_optim> to print:
	stats[0] = ind_final;
	stats[1] = ind_single;
	stats[2] = 1 + ind_prop2 - ind_prop1;



	return;
}
// ---------------------------------------------------------------------------

void TMainForm::Setup_Transfer(vector<double> &pfit, TParameterMatrix &pmat, int sdit, TPreparedDataContainer &datacal)
	// setting up everything for the call of the transfer function
{
	for (int p = 0; p < pfit.size(); p++)
	{
		NelminStart[p] = pfit[p];
		NelminXmin[p] = -1.0;
		NelminStep[p] = pfit[p] * 0.05;
		// 5% of input value used in <fminsearch> (MATLAB)
		if (NelminStep[p] < EPS)
			NelminStep[p] = 0.00025; // this value is also used in <fminsearch>
	}
	NelminN = pfit.size();

	Transfer_Params.PMat = pmat;
	Transfer_Params.SDIT = sdit;
	Transfer_Params.Data = datacal;
	Transfer_Params.FittingParamNumber = pfit.size();

	return;
}
// ---------------------------------------------------------------------------

double Transfer_External(double *x)
{
	vector<double>transfer_input(x, x + MainForm->Transfer_Params.FittingParamNumber);
	double out = MainForm->Transfer(transfer_input, MainForm->Transfer_Params.PMat, MainForm->Transfer_Params.SDIT,
		MainForm->Transfer_Params.Data, ERR);

	return out;
}
// ---------------------------------------------------------------------------

double TMainForm::Transfer(vector<double> &pfit, TParameterMatrix &pmat, int sdit, AnsiString &err)
{
	return Transfer(pfit, pmat, sdit, CalibData_All, err);
}
// ---------------------------------------------------------------------------

double TMainForm::Transfer(vector<double> &pfit, TParameterMatrix &pmat, int sdit, TPreparedDataContainer &datacal, AnsiString &err)
{
	Application->ProcessMessages();
	// Matlab Usage: minloglik = transfer(pfit,pmat,sdit,datacal_input)
	//
	// This is a transfer function which calculates the minus log-likelihood
	// function for a given set of parameters. This function is called by
	// <calc_parspace> and by <prepare_data> (for background hazard rate, via
	// <fminsearch>).
	//
	// This transfer function allows certain parameters to be fixed, while
	// others are fitted. The free parameters are in <pfit>, all parameters in
	// <pmat>, which contains in the second column a 1 where the parameter is to
	// be fitted. This format is required to work with the Matlab optimisation
	// routines (specifically <fminsearch>). Note that parameters in <pfit> and
	// in <pmat> are log-transformed where the fifth column of <pmat> is a 0
	// (including the parameters in <pmat> that are NOT fitted).
	//
	// Inputs
	// <pfit>          vector with to-be-fitted parameter values
	// <pmat>          the parameter matrix
	// <sdit>          switch for SD (1) or IT (2)
	// <datacal_input> input data set, ONLY used for calculating background
	// hazard rate in <prepare_data>
	//
	// Outputs
	// <minloglik>     minus log-likelihood associated with values in <pfit>
	//
	// Author     : Tjalling Jager



	// the following is already done in a function call without datacal input
	/* if isempty(datacal_input) // this input will be used when fitting background mortality; in that case, <transfer> will receive a dataset ...
	 datacal = CalibData_All; // use the global with the calibration data set
	 else
	 datacal = CalibData_HB; // otherwise, use the data set in the input (for background hazard rate)
	 end */

	double minloglik; // returning value
	TSimulationOutput x_out;
	TPreparedTreatmentSurv data_surv;
	TPreparedTreatmentConc ev_ic;
	vector<double>deathprob;
	vector<double>deathdata;

	//// BLOCK 1. First some work on the parameter matrix
	TParameterMatrix p_tmp = pmat;
	// copy the first column of the parameter matrix to a temporary parameter vector
	vector<int>ind_fit, ind_log;
	int count = 0;
	for (int p = 0; p < gutsPARAMETER_NO; p++)
	{
		if (pmat.Fit[p] == gutsFIT_PARAM)
		{
			ind_fit.push_back(p);
			// logical indices to fitted parameters (vector)
			p_tmp.Value[p] = pfit[count];
			// replace to-be-fitted values of the temporary vector with the values in <pfit>
			count++;
		}
		if (pmat.Scale[p] == gutsLOG_SCALE_PARAM)
			ind_log.push_back(p);
		// logical indices to log-scale parameters (vector)
	}

	// Note: the code below checks whether the parameter is within bounds on the
	// *log-transformed* value (rather than transforming the parameter itself
	// and checking with the normal bounds). Reason is that 10^(log10(x))-x is
	// not equal to x, but a small number (related to machine precision). This
	// means that when an element of <p_tmp> would be exactly on the bound, it
	// would still be seen as outside. This approach works better.

	// Put parameter bounds for parameters fitted on log scale on log scale.
	for (unsigned int p = 0; p < ind_log.size(); p++)
	{
		p_tmp.Min[ind_log[p]] = log10(p_tmp.Min[ind_log[p]]);
		p_tmp.Max[ind_log[p]] = log10(p_tmp.Max[ind_log[p]]);
	}
	// If any parameter is outside the bounds, return +inf as criterion.
	for (int p = 0; p < gutsPARAMETER_NO; p++)
		if (RoundTo(p_tmp.Value[p], -10) < RoundTo(p_tmp.Min[p], -10) || RoundTo(p_tmp.Value[p], -10) >
			RoundTo(p_tmp.Max[p], -10))
			// round function used because parameters on log scale could fail (value == max, but log(value) != log(max) possible!!)
		{
			minloglik = HIGH_POS_DOUBLE;
			// than make the <minloglik> infinite (very bad fit) // nelmin will throw an overflow if the minimum is too close to infinity (i.e. HIGH_POS_DOUBLE)
			return minloglik;
			// and return to the function calling this one (no need to run model)
		}
	// Put parameters that need to be fitted on log scale back on normal scale.
	for (unsigned int p = 0; p < ind_log.size(); p++)
	{
		if (p_tmp.Value[ind_log[p]] < log10(HIGH_POS_DOUBLE))
			p_tmp.Value[ind_log[p]] = pow(10.0, p_tmp.Value[ind_log[p]]);
		else
			p_tmp.Value[ind_log[p]] = HIGH_POS_DOUBLE;
	}
	// Note: <pmat> needs to be on normal scale for <calc_model>.

	//// BLOCK 2. Get model results
	// This block will call <calc_model> for the model predictions, and together
	// with the data set, turn that into a log-likelihood value. It has two
	// for-loops to loop over the data sets and over the individual treatments
	// in each data set.

	vector<double>loglik;
	loglik.resize(0); // initialise vector to catch all log-likelihoods

	for (unsigned int d = 0; d < datacal.Concentration.size(); d++) // run through all data sets
	{
		for (unsigned int c = 0; c < datacal.Concentration[d].size(); c++) // run through all exposure scenarios in this data set
		{
			// Extract the data block for survivors and exposure scenario for this treatment only.
			data_surv = datacal.Survival[d][c];
			// survival info from data (matrix)
			ev_ic = datacal.Concentration[d][c];
			// events matrix (three columns)

			if (data_surv.Times.size() > 0)
				// when the data set is not empty ...
			{
				// I think <data_surv> will be empty when fitting control data
				// on multiple data sets, where a control is missing in some
				// data sets. Since <loglik> is initialised with zeros, the
				// loglik on this position will remain zero, which is fine.

				x_out = Calc_Model(data_surv.Times, p_tmp.Value, ev_ic, sdit, err);
				if (err != "")
					return HIGH_POS_DOUBLE;
				// use <calc_model> to provide the output for this scenario
				// Note: <calc_model> is called with the first column of
				// <data_surv>, which contains the time vector for this block
				// (NaNs in the data set have been removed). The <x_out> will thus
				// only contain the predictions at the time points where there
				// is data.
				if (AbortCalibration || StopBatchPredictions)
				{
					return HIGH_POS_DOUBLE;
				}

				deathprob.resize(0);
				for (unsigned int i = 1; i < x_out.SurvivalProb.size();
				i++) // multinomial death probabilities from model for each interval (incl. interval after the test!)
						deathprob.push_back(Max(x_out.SurvivalProb[i - 1] - x_out.SurvivalProb[i], 1e-50));
				// otherwise we get problems taking the logarithm
				deathprob.push_back(Max(x_out.SurvivalProb[x_out.SurvivalProb.size() - 1], 1e-50));
				deathdata = data_surv.Deaths;
				// extract number of deaths from the data set
				// Note: this 1e-50 is unlikely to come into play as we set a
				// non-zero minimum for <hb>. Only for extremely short intervals
				// between observations may this be needed to avoid an error.

				double loglik_tmp = 0;
				for (unsigned int i = 0; i < deathdata.size(); i++)
				{
					if (loglik_tmp / 2.0 + deathdata[i] * log(deathprob[i]) / 2.0 > HIGH_NEG_DOUBLE / 2.0)
						loglik_tmp += deathdata[i] * log(deathprob[i]);
					// add negative term
					else
					{
						loglik_tmp = HIGH_NEG_DOUBLE;
						// else set it close to infinity (also negative; will be set positive at the end of the function)
						break;
					}
				}
				loglik.push_back(loglik_tmp);
				// and calculate the log likelihood
				// Note: this is vector product. What it does is element-wise
				// product of <deathdata> and ln-<deathprob>, and sum the
				// individual products.
			}
			else
				loglik.push_back(HIGH_NEG_DOUBLE);
		}
	}

	//// BLOCK 3. Final things

	minloglik = 0;
	for (int i = 0; i < loglik.size(); i++)
	{
		if (minloglik / 2.0 + loglik[i] / 2.0 > HIGH_NEG_DOUBLE / 2.0)
			minloglik += loglik[i]; // sums the log-likelihoods
		else
		{
			minloglik = HIGH_NEG_DOUBLE;
			break;
		}
	}
	minloglik *= -1.0; // and makes it negative
	// Note: this is needed as <fminsearch> is looking for a MINIMUM, and we
	// need to find the MAXIMUM of the likelihood function.

	// if isnan(minloglik) || ~isreal(minloglik) // when the loglik calculation returns NaN or complex numbers ...
	// minloglik = +inf; // give it a really bad likelihood value so the optimisation ignores it
	// I am not sure if this still occurs, but it does not hurt to check and
	// return a +inf.



	return minloglik;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::InputInspectDataImageMouseDown(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y)
{
	if (Button != mbRight)
		return;

	Image = (TImage*)Sender;
	Image->Picture->Assign(((TImage*)Sender)->Picture);
	TPoint mouse_position;
	mouse_position = Mouse->CursorPos;
	CopyImagePopupMenu->Popup(mouse_position.x, mouse_position.y);

	return;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::acFileProjectDescriptionExecute(TObject *Sender)
{
	ProjectDescriptionDlg->ShowModal();
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::PredictionsInspectDataImageMouseDown(TObject *Sender, TMouseButton Button, TShiftState Shift,
	int X, int Y)
{
	if (Button != mbRight)
		return;

	Image = (TImage*)Sender;

	TPoint mouse_position;
	mouse_position = Mouse->CursorPos;
	CopyImagePopupMenu->Popup(mouse_position.x, mouse_position.y);

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::Calc_Proflik(vector<vector<TParameterSpace> > &coll_prof, vector<TParameterSpace> &coll_ok, vector<double> &phat,
	double &mll, vector<vector<TParameterSpace> > &coll_prof_pruned, int &flag_short, TParameterMatrix pmat,
	vector<TParameterSpace> &coll_all, int sdit, TParameterMatrix &bnds_tmp)
{
	// Usage: [coll_prof,coll_ok,phat,mll,coll_prof_pruned,flag_short] = calc_proflik(pmat,coll_all,sdit,bnds_tmp)
	//
	// Use the collected sample from parameter space to zoom in on the profile
	// likelihood for the single parameters. This is useful to extract
	// single-parameter CIs, but also to extend the sample with profiled sets.
	// The latter may be useful when sampling is poor in some part of parameter
	// space. Furthermore, the profiling does a number of optimisations (one
	// parameter fixed), which may spot a better value of the optimum.
	//
	// Strategy is to split up the parameter range for each parameter into 50
	// slices of equal width. Within each slice, look for the best-fitting set.
	// Use that set as starting point for an optimisation (with the target
	// parameter fixed to the middle of the slice). Also do another optimisation
	// from the optimised value in the previous slice. Sometimes, extending a
	// profile works better than taking the best available sample in the slice.
	// From these two optimisations, the best value is optimised again and
	// stored. Next, the profiling starts from the other side and calulates the
	// next version from the previous profile point. In some cases, extending a
	// profile from the other direction can find a better profile.
	//
	// Additionally, the algorithm is looking for situations where there is a
	// substantial difference between the profile and the sample. If that
	// happens in a relevant range (where the fit is still acceptable), the
	// points are collected and will be used for an additional round of sampling
	// (apparently, sampling was not optimal in that case). Note that the entire
	// profile will also be added to the total sample as they are relevant
	// parameter sets (this happens in <calc_parspace> at the end, when making
	// CIs).
	//
	// Extra output is a pruned version of <coll_prof> that is more useful for
	// plotting and saving.
	//
	// Inputs
	// <pmat>      parameter matrix
	// <coll_all>  the entire sample (matrix)
	// <sdit>      switch for SD (1) or IT (2)
	// <bnds_tmp>  boundaries of parameter space (log-transformed were needed)
	//
	// Outputs
	// <coll_prof>  profile likelihoods with all parameters for each point (cell array)
	// <coll_ok>    matrix with all candidate parameter sets for resampling
	// <phat>       if the profiling found a better overall optimum, this
	// collects it (vector with fitted parameters only)
	// <mll>        this function also returns the MLL; usually, this is the top
	// value in <coll_all> (only different when a better optimum is
	// found)
	// <coll_prof_pruned>  pruned profile information; cell array with 2-column
	// matrix for each fitted parameter
	// <flag_short>        flag that is 1 when relevant part of the profile of
	// at least one parameter is too short (triggers
	// reprofiling)
	//
	// Author     : Tjalling Jager

	TMemo *memo;
	if (sdit == gutsSD)
		memo = CalibrationSDMemo;
	else
		memo = CalibrationITMemo;
	AnsiString as;

	TParameterMatrix edges_cloud; // edges of the joint 95% cloud (matrix)
	vector<double>edges_cloud_min_tmp;
	vector<double>edges_cloud_max_tmp;
	vector<TParameterSpace>par_best;
	// collect best better parameter set when profiling finds a better optimum

	int parnr, ind_best;
	vector<int>ind_fit_tst;
	vector<double>par_rng;
	// vector of evenly-spaced values across the parameters total range (final cloud)
	double a, b, n, spacing, gridsp;

	int ind_tst;
	vector<TParameterSpace>parprof;
	double mll_compare, mll_tst, mll_tst1, mll_tst2;
	vector<double>p_tst1, p_tst2, phat_tst, phat_tst1, phat_tst2;
	TParameterMatrix pmat_tst;
	TParameterSpace param_space_tmp;

	int flag_disp;
	vector<double>p_tst, pfit;

	int parnum, ind_lo, ind_hi;
	vector<TParameterSpace>coll_prof_tmp;
	coll_prof_tmp.resize(gutsPARAMETER_NO);
	vector<int>ind_outofbnds;

	//// BLOCK 1. Initial things.

	vector<int>ind_fit; // logical indices to fitted parameters (vector)
	ind_fit.resize(0);
	for (int p = 0; p < gutsPARAMETER_NO; p++)
		if (pmat.Fit[p] == gutsFIT_PARAM)
			ind_fit.push_back(p);

	int n_fit = 0; // number of fitted parameters
	for (int p = 0; p < gutsPARAMETER_NO; p++)
		if (pmat.Fit[p] == gutsFIT_PARAM)
			n_fit++;
	// n_par   = size(pmat,1);         // number of parameters in total
	int gridsz = 50;
	// number of points for the profile over the parmeter's range

	// opts_std   = SETTINGS_OPTIM.opts_std; // read standard options for <fminsearch>
	// Make options for rough simplex optimisations (based on number of fitted parameters) that will be done after several rounds
	// opts_rough = optimset(opts_std,'TolX',1e-2,'TolFun',1e-2,'MaxFunEvals',30*n_fit); // rough Simplex options

	mll = coll_all[0].Likelihood;
	// read the best min log-likelihood from first position of <coll_all>
	double mll_rem = mll;
	// remember the initial one entered (<mll> may be updated during profiling)
	double chicrit_single = 0.5 * gutsCRIT_TABLE[0];
	// criterion for single-parameter CIs
	double chicrit_joint = 0.5 * gutsCRIT_TABLE[ind_fit.size() - 1];
	// criterion for joint 95% CI or parameters
	chicrit_joint = Max(chicrit_joint, chicrit_single + 1.5);
	// makes sure to take a bit more than the single-par cut-off for 1 and 2 fitted parameters

	int ind_final = 0;
	for (int k = coll_all.size() - 1; k >= 0; k--)
		if (coll_all[k].Likelihood < mll + chicrit_joint)
			// find index to last point still in 95% CI
		{
			ind_final = k;
			break;
		}
	coll_prof.resize(gutsPARAMETER_NO);
	// initialise an empty cell array that will collect the profiles for plotting later
	coll_ok.resize(0);
	// start with an empty matrix to collect candidate sets that will go through another round of sampling
	par_best.resize(0); // initialise as empty

	edges_cloud_min_tmp.resize(gutsPARAMETER_NO);
	edges_cloud_max_tmp.resize(gutsPARAMETER_NO);
	for (int p = 0; p < gutsPARAMETER_NO; p++)
	{
		edges_cloud_min_tmp[p] = HIGH_POS_DOUBLE;
		edges_cloud_max_tmp[p] = HIGH_NEG_DOUBLE;
	}
	for (int i = 0; i < ind_final + 1; i++)
		for (int p = 0; p < gutsPARAMETER_NO; p++)
		{
			if (coll_all[i].Parameter[p] < edges_cloud_min_tmp[p])
				edges_cloud_min_tmp[p] = coll_all[i].Parameter[p];
			if (coll_all[i].Parameter[p] > edges_cloud_max_tmp[p])
				edges_cloud_max_tmp[p] = coll_all[i].Parameter[p];
		}
	edges_cloud.Min = edges_cloud_min_tmp;
	edges_cloud.Max = edges_cloud_max_tmp;

	//// BLOCK 2. Do the actual profiling.
	for (int p = 0; p < ind_fit.size(); p++) // run through all fitted parameters
	{
		// BLOCK 2.1. Initial things.
		parnr = ind_fit[p]; // index of this fitted parameter in <pmat>
		par_rng.resize(0);
		par_rng = EquidistantNumbers(edges_cloud.Min[parnr], edges_cloud.Max[parnr], gridsz);
		// par_rng = linspace(edges_cloud(parnr,1),edges_cloud(parnr,2),gridsz); // vector of evenly-spaced values across the parameters total range (final cloud)
		gridsp = 0.5 * (edges_cloud.Max[parnr] - edges_cloud.Min[parnr]) / (gridsz - 1);
		// half distance between points in the grid for this parameter

		// The profile needs to include the best value exactly, and we do this
		// by slightly shifting the range to the right.
		for (int k = par_rng.size() - 1; k >= 0; k--)
			if (par_rng[k] <= coll_all[0].Parameter[parnr])
			{
				ind_best = k;
				// find the value in the grid range just below the best value
				break;
			}
		for (int i = 0; i < par_rng.size(); i++)
			par_rng[i] += (coll_all[0].Parameter[parnr] - par_rng[ind_best]);
		// shift the range to include the best value

		// The variables below with a '1' at the end relate to the optimisations
		// from the best point in the sample. Variable with a '2' relate to
		// optimisations from the previous point on the profile.
		TParameterMatrix pmat_tst1 = pmat;
		// copy <pmat> to temporary matrix (which will be modified)
		pmat_tst1.Fit[parnr] = gutsNOT_FIT_PARAM;
		// don't fit this parameter as we are profiling it
		TParameterMatrix pmat_tst2 = pmat_tst1; // create another copy
		ind_fit_tst.resize(0);
		for (int i = 0; i < gutsPARAMETER_NO; i++)
			if (pmat_tst1.Fit[i] == gutsFIT_PARAM)
				ind_fit_tst.push_back(i);
		// new index to the parameters to be fitted (vector)

		// BLOCK 2.2. Run through the grid points and optimise.

		parprof.resize(0);
		// initialise an empty matrix to catch the profile (all parameters plus their likelihood)
		// parprof = nan(gridsz,n_par+1); // initialise an empty matrix to catch the profile (all parameters plus their likelihood)

		// The first thing to do is to run over the grid just made (based on the
		// sample) and optimise in each interval to find the best point (which
		// is the profile likelihood).
		for (int g = 0; g < gridsz; g++) // run through all grid points (the x-axis of the profile likelihood)
		{
			// BLOCK 2.2.1. Find best value from <coll_all> in this sub-range.
			// This parameter set will be used as starting values for
			// optimisation from the sample. Note that <coll_all> is sorted, so
			// the first within this range is automatically the best.
			ind_tst = -1;
			for (int k = 0; k < coll_all.size(); k++)
				if (coll_all[k].Parameter[parnr] > par_rng[g] - gridsp && coll_all[k].Parameter[parnr] < par_rng[g] + gridsp)
				{
					ind_tst = k;
					break;
				}

			if (ind_tst == -1)
				// it can be empty if there are no sample points in this sub-range
			{
				double abs_tmp = HIGH_POS_DOUBLE;
				for (int k = 0; k < coll_all.size(); k++)
					if (abs(coll_all[k].Parameter[parnr] - par_rng[g]) < abs_tmp)
					{
						abs_tmp = abs(coll_all[k].Parameter[parnr] - par_rng[g]);
						ind_tst = k; // just take the closest point available
					}
				mll_compare = HIGH_POS_DOUBLE;
				// and set MLL here to INF (so it will be flagged for a gap)
			}
			else
				mll_compare = coll_all[ind_tst].Likelihood;
			// this is the best MLL from the sample in this slice

			// BLOCK 2.2.2. Generate a profile point from this sample point.
			pmat_tst1.Value = coll_all[ind_tst].Parameter;
			// put parameter values for this point from the sample in the temporary <pmat_tst1>
			pmat_tst1.Value[parnr] = par_rng[g];
			// force the value on the grid for the parameter to be profiled
			p_tst1.resize(0);
			for (int i = 0; i < ind_fit_tst.size(); i++)
				p_tst1.push_back(pmat_tst1.Value[ind_fit_tst[i]]);
			// parameter estimates to be fitted for this point

			Setup_Transfer(p_tst1, pmat_tst1, sdit, CalibData_All);
			ERR = "";
			nelmin(Transfer_External, NelminN, NelminStart, NelminXmin, &NelminYnewlo, nelminROUGH_REQMIN, NelminStep,
				nelminKONVGE, optimROUGH_MAXFUNEVALS_PER_FITPARAM*NelminN, &NelminIcount, &NelminNumres, &NelminIfault);
			if (ERR != "")
				return;
			phat_tst1.resize(0);
			for (int p = 0; p < NelminN; p++)
				phat_tst1.push_back(NelminXmin[p]);
			mll_tst1 = NelminYnewlo;

			// BLOCK 2.2.3. Generate a profile point from the previous profile point.
			if (g > 0) // then we have a previous value on the profile line
			{
				// Then also use the previous value to see if propagating that one works better!
				pmat_tst2.Value = parprof[g - 1].Parameter;
				// put parameter values for this point from the profile in the temporary <pmat_tst2>
				pmat_tst2.Value[parnr] = par_rng[g];
				// force the value on the current grid point for the parameter to be profiled
				p_tst2.resize(0);
				for (int i = 0; i < ind_fit_tst.size(); i++)
					p_tst2.push_back(pmat_tst2.Value[ind_fit_tst[i]]);
				// parameter estimates to be fitted for this point

				Setup_Transfer(p_tst2, pmat_tst2, sdit, CalibData_All);
				ERR = "";
				nelmin(Transfer_External, NelminN, NelminStart, NelminXmin, &NelminYnewlo, nelminROUGH_REQMIN, NelminStep,
					nelminKONVGE, optimROUGH_MAXFUNEVALS_PER_FITPARAM*NelminN, &NelminIcount, &NelminNumres, &NelminIfault);
				if (ERR != "")
				return;
				phat_tst2.resize(0);
				for (int p = 0; p < NelminN; p++)
					phat_tst2.push_back(NelminXmin[p]);
				mll_tst2 = NelminYnewlo;
			}
			else
				mll_tst2 = HIGH_POS_DOUBLE;
			// otherwise, min-log-likelihood value from previously profiled point is INF

			// BLOCK 2.2.4. Compare the two estimates for the new profile point.
			if (mll_tst2 < mll_tst1)
				// if previously profiled leads to the better likelihood ...
			{
				phat_tst = phat_tst2;
				// use run from the previous profiled point
				pmat_tst = pmat_tst2;
			}
			else // otherwise ...
			{
				phat_tst = phat_tst1; // use run from sample point
				pmat_tst = pmat_tst1;
			}

			// BLOCK 2.2.5. Do a normal optimisation on the best value to make
			// sure we have the optimum.
			Setup_Transfer(phat_tst, pmat_tst, sdit, CalibData_All);
			ERR = "";
			nelmin(Transfer_External, NelminN, NelminStart, NelminXmin, &NelminYnewlo, nelminSTD_REQMIN, NelminStep, nelminKONVGE,
				optimSTD_MAXFUNEVALS_PER_FITPARAM*NelminN, &NelminIcount, &NelminNumres, &NelminIfault);
            if (ERR != "")
				return;
			phat_tst.resize(0);
			for (int p = 0; p < NelminN; p++)
				phat_tst.push_back(NelminXmin[p]);
			mll_tst = NelminYnewlo;
			for (int i = 0; i < ind_fit_tst.size(); i++)
				pmat_tst.Value[ind_fit_tst[i]] = phat_tst[i];
			// place the best-fitted parameters in <pmat_tst>
			param_space_tmp.Parameter = pmat_tst.Value;
			// and add the result to <parprof>
			param_space_tmp.Likelihood = mll_tst;
			parprof.push_back(param_space_tmp);

			// BLOCK 2.2.6. Check for gaps and better optima.
			if (mll_tst < mll + chicrit_single + 1)
				// only if we are not clearly above the parameter's CI ...
			{
				// We don't care when there is a gap between profile and sample
				// points in parts of parameter space that results in bad fits
				// anyway.

				// Do some tests to see if there are candidates for additional sampling
				if (mll_compare - mll_tst > optimGAP_EXTRA)
				{
					// There is a point, but the optimised value is better:
					// collect both the parameter values for the point and
					// the profile into <coll_ok>! Note: <mll_compare> will be INF
					// when there is no set in this slice at all. That is fine:
					// closest sample point and profile point will be collected.
					coll_ok.push_back(coll_all[ind_tst]);
					coll_ok.push_back(param_space_tmp);
				}
				if (mll_tst < mll)
					// then we found a better optimum, so collect it
				{
					par_best.resize(1);
					par_best[0] = parprof[g];
					if (mll_rem - mll_tst > optimREAL_BETTER && mll - mll_tst > optimREAL_BETTER)
					{
						// Only print on screen when the new optimum is really
						// better than the original one, and better than the one
						// that is now the best.
						as = "  Better optimum found when profiling " + AnsiString(gutsPARAMETERS[parnr].c_str()) + ": " +
							AnsiString(mll_tst) + " (best was " + AnsiString(mll) + ")";
						memo->Lines->Add(as);
					}
					mll = mll_tst;
				}
			}
		}

		// BLOCK 2.3. Run through the profile in reverse to see if it can be improved.
		for (int g = gridsz - 2; g >= 0; g--) // run through all grid points (the x-axis of the profile likelihood) in reverse!
		{
			mll_tst1 = parprof[g].Likelihood;
			// MLL at this point of the profile
			pmat_tst2.Value = parprof[g + 1].Parameter;
			// put parameter values for the previous point from the profile in <pmat_tst2> (we run in reverse)
			pmat_tst2.Value[parnr] = parprof[g].Parameter[parnr];
			// force the value on the current value for the grid for the parameter to be profiled

			p_tst2.resize(0);
			for (int i = 0; i < ind_fit_tst.size(); i++)
				p_tst2.push_back(pmat_tst2.Value[ind_fit_tst[i]]);
			// parameter estimates to be fitted for this point

			Setup_Transfer(p_tst2, pmat_tst2, sdit, CalibData_All);
			ERR = "";
			nelmin(Transfer_External, NelminN, NelminStart, NelminXmin, &NelminYnewlo, nelminSTD_REQMIN, NelminStep, nelminKONVGE,
				optimSTD_MAXFUNEVALS_PER_FITPARAM*NelminN, &NelminIcount, &NelminNumres, &NelminIfault);
			if (ERR != "")
				return;
			phat_tst2.resize(0);
			for (int p = 0; p < NelminN; p++)
				phat_tst2.push_back(NelminXmin[p]);
			mll_tst2 = NelminYnewlo;
			for (int i = 0; i < ind_fit_tst.size(); i++)
				pmat_tst2.Value[ind_fit_tst[i]] = phat_tst2[i];
			// place the best-fitted parameters in <pmat_tst2>

			if (mll_tst2 < mll_tst1)
				// profiling from right to left provided a better value
			{
				parprof[g].Parameter = pmat_tst2.Value;
				// replace this entry in <parprof>
				parprof[g].Likelihood = mll_tst2;
				if (mll_tst1 - mll_tst2 > optimGAP_EXTRA)
				{
					// If the optimised value is really better: collect the profiled set into <coll_ok>!
					coll_ok.push_back(parprof[g]);
				}
				if (mll_tst2 < mll)
					// then we even found a better optimum, so collect it
				{
					par_best.resize(1);
					par_best[0] = parprof[g];
					if (mll_rem - mll_tst2 > optimREAL_BETTER && mll - mll_tst2 > optimREAL_BETTER)
					{
						// Only print on screen when the new optimum is really
						// better than the original one, and better than the one
						// that is now the best.
						as = "  Better optimum found when reverse profiling " + AnsiString(gutsPARAMETERS[parnr].c_str()) + ": " +
							AnsiString(mll_tst2) + " (best was " + AnsiString(mll) + ")";
						memo->Lines->Add(as);
					}
					mll = mll_tst2;
				}
			}
		}

		// BLOCK 2.4. Extend profiles to lower/higher values. In some cases, the
		// sample itself may not have caught the entire profile likelihood. When
		// this happens, the edges of the profile that was made so far
		// (collected in <parprof>) will not be above the chi2 criterion for the
		// single parameters. The strategy is to continue profiling to lower
		// and/or higher values of the parameter.
		//
		// Note: Extending the profile is done in steps of <gridsp>, which is
		// HALF of the distance between the regular profile points. This implies
		// that extension is done on a finer grid. This was a mistake on my
		// side, but I think it is okay to keep it in: extension is being done
		// without the aid of sample points, so maybe a finer grid is a good
		// idea. Will be a bit slow though. Also note that extension to lower
		// values will generally be triggered when the sample is on the lower
		// edge. This is caused by the fact that I shift the profiling grid
		// slightly to higher values to catch the best value exactly. That is
		// also not problematic.

		// BLOCK 2.4.1. Start with profiling to lower values of the parameter,
		// if needed. If we are not clearly above the parameter's CI yet, and
		// haven't hit the lower bound ... continu further down until we are, or
		// until we hit a boundary
		flag_disp = 0;
		// flag to make sure we only display the status on screen once
		while (parprof[0].Parameter[parnr] > bnds_tmp.Min[parnr] && parprof[0].Likelihood < mll + chicrit_single + 1)
		{
			if (flag_disp == 0)
			{
				as = "  Extending profile for " + AnsiString(gutsPARAMETERS[parnr].c_str()) + " to lower parameter values";
				memo->Lines->Add(as);
				flag_disp = 1;
				// set flag to one so we don't continue to print on screen
			}

			pmat_tst.Value = parprof[0].Parameter;
			// use first entry of <parprof> to continue with
			pmat_tst.Value[parnr] = pmat_tst.Value[parnr] - gridsp;
			// continue on the same grid spacing to lower value for this parameter
			pmat_tst.Value[parnr] = Max(pmat_tst.Value[parnr], bnds_tmp.Min[parnr]);
			// but make sure it is not below lower bound
			p_tst.resize(0);
			for (unsigned int i = 0; i < ind_fit_tst.size(); i++)
				p_tst.push_back(pmat_tst.Value[ind_fit_tst[i]]);
			// parameter estimates to be fitted for this point

			// Since we are extending the profile into a difficult area of
			// parameter space, do two optimisations: rough followed by
			// normal.

			// first do a rough optimisation
			Setup_Transfer(p_tst, pmat_tst, sdit, CalibData_All);
			ERR = "";
			nelmin(Transfer_External, NelminN, NelminStart, NelminXmin, &NelminYnewlo, nelminROUGH_REQMIN, NelminStep,
				nelminKONVGE, optimROUGH_MAXFUNEVALS_PER_FITPARAM*NelminN, &NelminIcount, &NelminNumres, &NelminIfault);
			if (ERR != "")
				return;
			phat_tst.resize(0);
			for (int p = 0; p < NelminN; p++)
				phat_tst.push_back(NelminXmin[p]);

			// then do a normal optimisation with prefitted values
			Setup_Transfer(phat_tst, pmat_tst, sdit, CalibData_All);
			ERR = "";
			nelmin(Transfer_External, NelminN, NelminStart, NelminXmin, &NelminYnewlo, nelminSTD_REQMIN, NelminStep, nelminKONVGE,
				optimSTD_MAXFUNEVALS_PER_FITPARAM*NelminN, &NelminIcount, &NelminNumres, &NelminIfault);
			if (ERR != "")
				return;
			phat_tst.resize(0);
			for (int p = 0; p < NelminN; p++)
				phat_tst.push_back(NelminXmin[p]);
			mll_tst = NelminYnewlo;

			for (int i = 0; i < ind_fit_tst.size(); i++)
				pmat_tst.Value[ind_fit_tst[i]] = phat_tst[i];
			// replace values in <pmat_tst> with fitted ones
			param_space_tmp.Parameter = pmat_tst.Value;
			param_space_tmp.Likelihood = mll_tst;
			parprof.insert(parprof.begin(), param_space_tmp);
			// and add new entry *on top* of <parprof>
			coll_ok.push_back(parprof[0]);
			// also add this additional value to <coll_ok> as they are candidates for extra sampling

			if (mll_tst < mll)
				// then we even found a better optimum, so collect it
			{
				par_best.resize(1);
				par_best[0] = parprof[0];
				if (mll_rem - mll_tst > optimREAL_BETTER && mll - mll_tst > optimREAL_BETTER)
				{
					// Only print on screen when the new optimum is really
					// better than the original one, and better than the one
					// that is now the best.
					as = "  Better optimum found when extending profile for " + AnsiString(gutsPARAMETERS[parnr].c_str()) +
						" down: " + AnsiString(mll_tst) + " (best was " + AnsiString(mll) + ")";
					memo->Lines->Add(as);
				}
				mll = mll_tst; // update MLL
			}
		}

		// BLOCK 2.4.2. Do the same thing to higher values of the parameter, if
		// needed. If we are not clearly above the parameter's CI, and haven't
		// hit the upper bound ... continu further up until we are, or until we
		// hit a boundary.
		flag_disp = 0;
		// flag to make sure we only display the status on screen once
		while (parprof[parprof.size() - 1].Parameter[parnr] < bnds_tmp.Max[parnr] && parprof[parprof.size() - 1].Likelihood <
			mll + chicrit_single + 1)
		{
			if (flag_disp == 0)
			{
				memo->Lines->Add("  Extending profile for " + AnsiString(gutsPARAMETERS[parnr].c_str()) +
					" to higher parameter values");
				flag_disp = 1;
				// set flag to one so we don't continue to print on screen
			}

			pmat_tst.Value = parprof[parprof.size() - 1].Parameter;
			// use last entry of <parprof> to continue with
			pmat_tst.Value[parnr] += gridsp;
			// continue on the same grid spacing to higher value for this parameter
			pmat_tst.Value[parnr] = Min(pmat_tst.Value[parnr], bnds_tmp.Max[parnr]);
			// make sure it is not above upper bound
			p_tst.resize(0);
			for (int i = 0; i < ind_fit_tst.size(); i++)
				p_tst.push_back(pmat_tst.Value[ind_fit_tst[i]]);
			// parameter estimates to be fitted for this point

			// Two optimisation rounds.

			// first do a rough optimisation
			Setup_Transfer(p_tst, pmat_tst, sdit, CalibData_All);
			ERR = "";
			nelmin(Transfer_External, NelminN, NelminStart, NelminXmin, &NelminYnewlo, nelminROUGH_REQMIN, NelminStep,
				nelminKONVGE, optimROUGH_MAXFUNEVALS_PER_FITPARAM*NelminN, &NelminIcount, &NelminNumres, &NelminIfault);
            if (ERR != "")
				return;
			phat_tst.resize(0);
			for (int p = 0; p < NelminN; p++)
				phat_tst.push_back(NelminXmin[p]);

			// then do a normal optimisation with prefitted values
			Setup_Transfer(phat_tst, pmat_tst, sdit, CalibData_All);
			ERR = "";
			nelmin(Transfer_External, NelminN, NelminStart, NelminXmin, &NelminYnewlo, nelminSTD_REQMIN, NelminStep, nelminKONVGE,
				optimSTD_MAXFUNEVALS_PER_FITPARAM*NelminN, &NelminIcount, &NelminNumres, &NelminIfault);
			if (ERR != "")
				return;
			phat_tst.resize(0);
			for (int p = 0; p < NelminN; p++)
				phat_tst.push_back(NelminXmin[p]);
			mll_tst = NelminYnewlo;

			for (int i = 0; i < ind_fit_tst.size(); i++)
				pmat_tst.Value[ind_fit_tst[i]] = phat_tst[i];
			// replace values in <pmat_tst> with fitted ones
			param_space_tmp.Parameter = pmat_tst.Value;
			param_space_tmp.Likelihood = mll_tst;
			parprof.push_back(param_space_tmp);
			// add new entry *on bottom* of <parprof>
			coll_ok.push_back(parprof[parprof.size() - 1]);
			// also add this additional value to <coll_ok> as they are candidates for extra sampling

			if (mll_tst < mll)
				// then we even found a better optimum, so collect it
			{
				par_best.resize(1);
				par_best[0] = parprof[parprof.size() - 1];
				if (mll_rem - mll_tst > optimREAL_BETTER && mll - mll_tst > optimREAL_BETTER)
				{
					// Only print on screen when the new optimum is really
					// better than the original one, and better than the one
					// that is now the best.
					memo->Lines->Add("  Better optimum found when extending profile for " +
						AnsiString(gutsPARAMETERS[parnr].c_str()) + " up: " + AnsiString(mll_tst) + " (best was " +
						AnsiString(mll) + ")");
				}
				mll = mll_tst; // update MLL
			}
		}
		coll_prof[parnr] = parprof;
		// collect this <parprof> in the cell array <coll_prof> (for output)
	}

	//// BLOCK 3. Check if a better optimum was found, and if so, refine it.

	if (par_best.size() > 0) // if profiling led to a better optimum
	{
		memo->Lines->Add("  Finished profiling, running a simplex optimisation on the best fit set found ...");

		// Do another optimisation as profiling has detected a better optimum.
		pmat.Value = par_best[0].Parameter;
		// update <pmat> with the best value found during profiling
		pfit.resize(0);
		for (int i = 0; i < ind_fit.size(); i++)
			pfit.push_back(pmat.Value[ind_fit[i]]);
		// copy best values to <pfit>, for parameters that are to be fitted

		Setup_Transfer(pfit, pmat, sdit, CalibData_All);
		ERR = "";
		nelmin(Transfer_External, NelminN, NelminStart, NelminXmin, &NelminYnewlo, nelminSTD_REQMIN, NelminStep, nelminKONVGE,
			optimSTD_MAXFUNEVALS_PER_FITPARAM*NelminN, &NelminIcount, &NelminNumres, &NelminIfault);
		if (ERR != "")
			return;
		phat.resize(0);
		for (int p = 0; p < NelminN; p++)
			phat.push_back(NelminXmin[p]);
		mll = NelminYnewlo;
	}
	else
		phat.resize(0);
	// signal <calc_parspace> that we have NOT found a better value

	//// BLOCK 4. Prune <coll_prof> for later plotting and saving
	// The cell array <coll_prof> contains an n-by-6 matrix for each parameter.
	// It contains all of the complete parameter sets that make up the profile
	// likelihoods. This is handy when we want to resample from a set on the
	// profile, to see if hitting parameters bounds for one parameter affects
	// the CI of another parameter, and to augment the sample with the profiled
	// sets. However, for plotting and saving, we can use a reduced set: only
	// the x and y values for the profile (x values are values of the parameter
	// of interest, y values are the associated MLL). The first pruning step is
	// thus to remove the information on the non-profiled parameters from the
	// array.
	//
	// The second step of pruning is to remove parts of the profile that we are
	// not interested in. Everything beyond the chi2 cut-off for the 95% joint
	// CI of the parameters can be removed. Normally, profiling won't go there.
	// However, some part of the profile may become irrelevant after we found a
	// better optimum, so that needs to be removed. This could leave the profile
	// short (few points, which implies low resolution), which will flag
	// re-profiling in <calc_parspace>.

	flag_short = 0; // if nothing happens, flag is zero
	coll_prof_pruned.resize(gutsPARAMETER_NO); // create empty cell array
	// coll_prof_pruned = cell(length(ind_fit),1); // create empty cell array

	for (unsigned int i = 0; i < ind_fit.size(); i++) // run through the fitted parameters
	{
		parnum = ind_fit[i];
		// find the parameter number for this fitted parameter
		coll_prof_tmp = coll_prof[parnum];
		// create a temporary matrix with the x and y values of this profile
		// coll_prof_tmp = [coll_prof{parnum}(:,parnum) coll_prof{parnum}(:,end)]; // create a temporary matrix with the x and y values of this profile
		// This is step 1: removing info on other parameters than the profiled one.

		ind_outofbnds.resize(0);
		for (unsigned int j = 0; j < coll_prof_tmp.size(); j++)
			if (coll_prof_tmp[j].Parameter[parnum] > bnds_tmp.Max[parnum] || coll_prof_tmp[j].Parameter[parnum] <
				bnds_tmp.Min[parnum])
				ind_outofbnds.push_back(j);
		// ind_outofbnds = coll_prof_tmp(:,1) > bnds_tmp(parnum,2) | coll_prof_tmp(:,1) < bnds_tmp(parnum,1); // indices to parts of the profiles that cross the min-max bounds
		for (int j = ind_outofbnds.size() - 1; j >= 0; j--)
			coll_prof_tmp.erase(coll_prof_tmp.begin() + ind_outofbnds[j]);
		// coll_prof_tmp(ind_outofbnds,:) = [];

		// In some cases, we have tails on the profiles that extend to very
		// high values of the MLL (because we found a better optimum
		// during/after profiling). These tails can be pruned in step 2.
		if (n_fit > 1)
		{
			ind_lo = -1;
			for (unsigned int k = 0; k < coll_prof_tmp.size(); k++)
				if (coll_prof_tmp[k].Likelihood < mll + chicrit_joint)
				{
					ind_lo = k;
					// index to first entry that is within the cut-off
					break;
				}
			ind_hi = -1;
			for (int k = coll_prof_tmp.size() - 1; k >= 0; k--)
				if (coll_prof_tmp[k].Likelihood < mll + chicrit_joint)
				{
					ind_hi = k;
					// index to last entry that is within the cut-off
					break;
				}
		}
		else
		{
			// Note: an extra 0.5 is needed when we are fitting only 1
			// parameter; otherwise, the profile is cut short (as the joint
			// criterion is the same as the single criterion).
			ind_lo = -1;
			for (unsigned int k = 0; k < coll_prof_tmp.size(); k++)
				if (coll_prof_tmp[k].Likelihood < mll + chicrit_joint + 0.5)
				{
					ind_lo = k;
					// index to first entry that is within the cut-off
					break;
				}
			ind_hi = -1;
			for (int k = coll_prof_tmp.size() - 1; k >= 0; k--)
				if (coll_prof_tmp[k].Likelihood < mll + chicrit_joint + 0.5)
				{
					ind_hi = k;
					// index to last entry that is within the cut-off
					break;
				}
		}

		ind_lo = Max(0, ind_lo - 1); // take one extra set lower if there is one
		ind_hi = Min(coll_prof_tmp.size() - 1, ind_hi + 1);
		// take one extra set higher if there is one

		coll_prof_pruned[ind_fit[i]].resize(0);
		for (int j = ind_lo; j < ind_hi + 1; j++)
			coll_prof_pruned[parnum].push_back(coll_prof_tmp[j]);
		// only keep the ones without any extreme tails
		// Note: this procedure only keeps the sets within the criterion, and
		// adds one lower and higher. This last step is needed for extremely
		// steep profiles.

		for (unsigned int j = 0; j < coll_prof_pruned[parnum].size(); j++)
			if (coll_prof_pruned[parnum][j].Likelihood > HIGH_POS_DOUBLE - 1)
				coll_prof_pruned[parnum][j].Likelihood = mll + 100;
		// replace any INF with a very high value
		// Note: this implies that this part of the profile will be plotted as a
		// very steep line. Further, in <calc_parspace>, this will allow
		// interpolation rather than taking the extreme value. This improves the
		// CIs a little bit when a profile is extremely steep.

		// If, after pruning, the profile is rather short, flag that, so
		// <calc_parspace> will do an new round of profiling.
		if (coll_prof_pruned[parnum].size() < 40) // 40 seems good enough
				flag_short = 1;
	}

	// store coll_prof_pruned in global variable for plotting
	if (sdit == gutsSD)
		CalibrationProfileLikelihoodsSD = coll_prof_pruned;
	else
		CalibrationProfileLikelihoodsIT = coll_prof_pruned;

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::Test_Proflik(vector<double> &flag_profile, vector<TParameterSpace> &coll_ok, TParameterMatrix &pmat,
	vector<vector<TParameterSpace> > &coll_prof, vector<TParameterSpace> &coll_all, AnsiString &err)
{

	// Matlab Usage: [flag_profile,coll_ok] = test_proflik(pmat,coll_prof,coll_all)
	//
	// Testing of the profile likelihood, using the sample of sets. This
	// function looks for two situations:
	// 1) Points where the MLL of the sample is considerably higher (less good)
	// than the MLL of the profile. This indicates that the sampling has been
	// poor, so we need to resample later.
	// 2) Points where the MLL of the sample is considerably lower (better fit)
	// than the profile. This indicates that the profiling was not good
	// enough and may need to be redone.
	//
	// Inputs
	// <pmat>      parameter matrix
	// <coll_prof> profile likelihoods with all parameters for each point (cell array)
	// <coll_all>  the entire sample (matrix)
	//
	// Output
	// <flag_profile> two flags for checking whether reprofiling is needed (at
	// this moment, a vector with the number of points flagged and the
	// maximum MLL distance that was found for these points)
	// <coll_ok>      matrix to collect candidate parameter sets for resampling
	//
	// Author     : Tjalling Jager

	//// BLOCK 1. Initial things.

	vector<int>ind_fit; // logical indices to fitted parameters (vector)
	ind_fit.resize(0);
	for (int p = 0; p < gutsPARAMETER_NO; p++)
		if (pmat.Fit[p] == gutsFIT_PARAM)
			ind_fit.push_back(p);
	coll_ok.resize(0);
	// initialise empty matrix to collect candidates for resampling
	flag_profile[0] = 0.0; // initialise vector for flags with zeros
	flag_profile[1] = 0.0;
	double chicrit_single = 0.5 * gutsCRIT_TABLE[0];
	// criterion for single-parameter CIs
	double mll = coll_all[0].Likelihood; // maximum likelihood value so far

	//// BLOCK 2. Perform the checks, running through all fitted parameters.
	// We run through slices of the parameter space again. The profile
	// likelihoods are regularly spaced. As we step through the profiled points,
	// we consider the slice from parameter space that is halfway between the
	// previous point and the next in the profile.

	int parnr;
	vector<TParameterSpace>parprof;
	vector<TParameterSpace>coll_tst;
	double min_coll_tst, min_parprof, max_coll_tst, max_parprof;
	for (unsigned int p = 0; p < ind_fit.size(); p++) // run through all fitted parameters
	{
		// BLOCK 2.1. Initial things
		parnr = ind_fit[p]; // index of this fitted parameter in <pmat>
		parprof = coll_prof[parnr];
		// read profile information for this parameter from the ones entered (matrix with 6 columns)

		// There can be (extreme) cases where a profile for one parameter is
		// entirely on the wrong place (when profiling has led to a better
		// optimum, but extension of a certain parameter was not triggered).
		// This section checks whether the current profile in <parprof> captures
		// the relevant part of the current sample in <coll_all>. If not, a
		// reprofiling is triggered.
		min_coll_tst = HIGH_POS_DOUBLE;
		max_coll_tst = HIGH_NEG_DOUBLE;
		coll_tst.resize(0);
		for (unsigned int i = 0; i < coll_all.size(); i++)
			if (coll_all[i].Likelihood < mll + chicrit_single + 1.0)
				// make a test matrix with the part that is most relevant (not too far from inner rim)
			{
				coll_tst.push_back(coll_all[i]);
				if (coll_tst[coll_tst.size() - 1].Parameter[parnr] < min_coll_tst)
					min_coll_tst = coll_tst[coll_tst.size() - 1].Parameter[parnr];
				if (coll_tst[coll_tst.size() - 1].Parameter[parnr] > max_coll_tst)
					max_coll_tst = coll_tst[coll_tst.size() - 1].Parameter[parnr];
			}
		min_parprof = HIGH_POS_DOUBLE;
		max_parprof = HIGH_NEG_DOUBLE;
		for (unsigned int i = 0; i < parprof.size(); i++)
		{
			if (parprof[i].Parameter[parnr] < min_parprof)
				min_parprof = parprof[i].Parameter[parnr];
			if (parprof[i].Parameter[parnr] > max_parprof)
				max_parprof = parprof[i].Parameter[parnr];
		}
		if (min_coll_tst < min_parprof || max_coll_tst > max_parprof)
		{
			flag_profile[0]++; // count another problem
			flag_profile[1] = HIGH_POS_DOUBLE;
			// make sure to flag this as a big problem!
		}

		// Note: there are some cases where this part will be triggered again
		// and again. To solve this, more detail in the profile would be needed.
		// For now, this is 'solved' by the fact that only 2 reprofiles are
		// allowed before stopping the extra sampling.

		for (unsigned int i = 0; i < parprof.size(); i++)
			if (parprof[i].Likelihood >= mll + chicrit_single + 1.0)
			{
				parprof.erase(parprof.begin() + i);
				// only keep the part that is most relevant (not too far from inner rim)
				i--;
			}
		// This avoids refinement in areas that we're not interested in.

		vector<double>gridsp;
		gridsp.resize(2);
		int ind_tst;
		double mll_compare, mll_prof, min_MLL;
		if (parprof.size() < 1)
		{
			err += "Optimisation failed, this data set requires some specialist attention. ";
			return;
        }
		for (unsigned int g = 1; g < parprof.size() - 1; g++) // run through grid, but skip first and last (when it hits an edge, strange things can happen)
		{
			// BLOCK 2.2. Find best value from <coll_all> in this slice. This
			// parameter set will be compared to the profile likelihood. Note
			// that <coll_all> is sorted, so the first within this range is
			// automatically the best.
			//
			// Note: the spacing between the profile points is not necessarily
			// the same everywhere. Profiles may hit a boundary (and are then
			// placed on top of that boundary) or may be extended at half the
			// default grid spacing. Therefore, it is better to take half the
			// distance between the adjacent points. The <gridsp> is thus a
			// two-element vector with the half distance to the previous point
			// and to the next one. Since grid spacing is smaller when the
			// profile has been extended, it will probably trigger more gaps
			// than is strictly needed, and thus too much resampling. Only
			// downside is a larger sample and longer calculation times, but
			// that should be acceptable (since this will be a tough parameter
			// space anyway).
			// ++-- check following line
			gridsp[0] = 0.5 * (parprof[g].Parameter[parnr] - parprof[g - 1].Parameter[parnr]);
			// vector with half distance to previous and next point!
			gridsp[1] = 0.5 * (parprof[g + 1].Parameter[parnr] - parprof[g].Parameter[parnr]);

			ind_tst = -1;
			for (unsigned int k = 0; k < coll_all.size(); k++)
				if (coll_all[k].Parameter[parnr] > parprof[g].Parameter[parnr] - gridsp[0] && coll_all[k].Parameter[parnr] <
					parprof[g].Parameter[parnr] + gridsp[1])
				{
					ind_tst = k;
					break;
				}
			if (ind_tst == -1)
				// it can be empty if there are no sample points in this sub-range (which would be very odd)
			{
				double abs_tmp = HIGH_POS_DOUBLE;
				for (unsigned int k = 0; k < coll_all.size(); k++)
					if (abs(coll_all[k].Parameter[parnr] - parprof[g].Parameter[parnr]) < abs_tmp)
					{
						abs_tmp = abs(coll_all[k].Parameter[parnr] - parprof[g].Parameter[parnr]);
						ind_tst = k; // just take the closest point available
					}
				mll_compare = HIGH_POS_DOUBLE;
				// and set MLL here to INF (so it will be flagged for a gap)
			}
			else
				mll_compare = coll_all[ind_tst].Likelihood;
			// this is the best MLL from the sample in this slice
			mll_prof = parprof[g].Likelihood;
			// this is the MLL from the profile

			if (mll_prof < mll_compare)
				// then the profile is better than the sample (as it should be)
			{
				if (mll_compare - mll_prof > optimGAP_EXTRA)
					// then the gap is too wide for comfort
				{
					coll_ok.push_back(coll_all[ind_tst]);
					// put the sample point and the profile point in <coll_ok> for refinement
					coll_ok.push_back(parprof[g]);
				}
			}
			else if (mll_compare < mll_prof)
				// then the sample is better than the profile (which implies a need for new profiling)
			{
				// The profile is on a limited number of grid points on the
				// parameter axis, while the sample is anywhere in the slice.
				// This can be done complex, but it is probably enough to only
				// look at the minimum of the current profile point, previous
				// one, and next one. If the sample is below the lowest of the
				// three, it deserves to be counted as a true deviation.
				//
				// Note: one could decide to also put these problem areas into
				// <coll_ok>. I had that in a previous version as a test, but
				// decided not to do it anymore (can't remember why, though).

				min_MLL = Min(parprof[g-1].Likelihood, parprof[g].Likelihood);
				min_MLL = Min(min_MLL, parprof[g+1].Likelihood);
				// this is the smallest of previous, current and next MLL of the profile

				if (mll_compare < min_MLL)
					// if the sample is better than the lower edge of the profile in this slice ...
				{
					flag_profile[0]++; // count another problem
					flag_profile[1] = Max(flag_profile[1], min_MLL - mll_compare);
					// and collect the degree of difference if it is bigger than the previous
				}
			}
		}
	}

	return;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::MainPageControlChanging(TObject *Sender, bool &AllowChange)
{
	if (MainPageControl->TabIndex == 0)
	{
		AllowChange = CheckAndGetCalibrationData(TestCalibrationData);	// only allow change if valid input data is available
		if (AllowChange && !CalibrationPerformed)
			ResetCalibrationSettingInGUI();
	}

	return;
}
// ---------------------------------------------------------------------------

bool TMainForm::UpdateParameterRangesBasedOnInputData()
{
	/*// don't update if parameter ranges were manipulated
	if (ParameterRangesManipulated)
		return true;*/

	// get parameter ranges
	if (!DeriveParamRangesFromData())
		return false;

	// display the matrix in the GUI
	bool change_value;
	if (SDParamFSValueEdit->Text == "")  // not changeable parameter value (if "" it has not been evaluated yet)
		change_value = true;
	else
		change_value = false;
	bool change_range = true;
	DisplayParameterMatrixInGUI(PMatSD, gutsSD, change_value, change_range);
	DisplayParameterMatrixInGUI(PMatIT, gutsIT, change_value, change_range);

	//ParameterRangesManipulated = false;

	return true;
}
// ---------------------------------------------------------------------------

bool TMainForm::CreateBasicInformationReport(TWordApplication* word_application,
		TWordDocument *word_document, AnsiString &error)
{
	WordRangePtr selection;

	// headline
	SetReportFontTitle(word_application, word_document);
	AddCaptionToEndOfDocument(word_application, word_document, AnsiString(SOFTWARE_NAME) + " Report", error);

	AddParagraphToEndOfDocument(word_application, word_document);

	// project title
	SetReportFontBoldUnderlined(word_application, word_document);
	AddCaptionToEndOfDocument(word_application, word_document, "Project:", error);
	SetReportFontStandard(word_application, word_document);
	AddCaptionToEndOfDocument(word_application, word_document, "\t" + AnsiString(ProjectTitle.c_str()), error);

	// project file
	SetReportFontBoldUnderlined(word_application, word_document);
	AddCaptionToEndOfDocument(word_application, word_document, "Project file:", error);
	SetReportFontStandard(word_application, word_document);
	if (ProjectFileName.Length() > 0)
		AddCaptionToEndOfDocument(word_application, word_document, "\t" + AnsiString(ProjectFileName), error);
	else
		AddCaptionToEndOfDocument(word_application, word_document, "\tNo project file saved or loaded", error);

	// project description
	SetReportFontBoldUnderlined(word_application, word_document);
	AddCaptionToEndOfDocument(word_application, word_document, "Project description (optional):", error);
	SetReportFontStandard(word_application, word_document);
	if (ProjectDescription.length() > 0)
		AddCaptionToEndOfDocument(word_application, word_document, "\t" + AnsiString(ProjectDescription.c_str()), error);
	else
		AddCaptionToEndOfDocument(word_application, word_document, "\tNo project description available", error);

	// software version
	SetReportFontBoldUnderlined(word_application, word_document);
	AddCaptionToEndOfDocument(word_application, word_document, "Software version:", error);
	SetReportFontStandard(word_application, word_document);
	AddCaptionToEndOfDocument(word_application, word_document, "\t" + AnsiString(SOFTWARE_NAME) + " - " + AnsiString(SOFTWARE_VERSION), error);

	// Date/time
	SetReportFontBoldUnderlined(word_application, word_document);
	AddCaptionToEndOfDocument(word_application, word_document, "Date of report creation:", error);
	SetReportFontStandard(word_application, word_document);
	AddCaptionToEndOfDocument(word_application, word_document, "\t" + CurrTimeToUnixTime(), error);

	return true;
}
// ---------------------------------------------------------------------------

AnsiString TMainForm::CurrTimeToUnixTime()
// returns: "26/04/2013 16:11:45"
{
	time_t now = time(0);
	tm *tm_struct=localtime(&now);
	AnsiString day=IntToStr(tm_struct->tm_mday);
	if (day.Length()<2) day="0"+day;
	AnsiString mon=IntToStr(tm_struct->tm_mon+1);
	if (mon.Length()<2) mon="0"+mon;
	AnsiString hour=IntToStr(tm_struct->tm_hour);
	if (hour.Length()<2) hour="0"+hour;
	AnsiString min=IntToStr(tm_struct->tm_min);
	if (min.Length()<2) min="0"+min;
	AnsiString sec=IntToStr(tm_struct->tm_sec);
	if (sec.Length()<2) sec="0"+sec;

	return day+"/"+mon+"/"+IntToStr(tm_struct->tm_year+1900)+" "+hour+":"+min+":"+sec;
}
// ---------------------------------------------------------------------------

void TMainForm::AddInputDataToReport(TWordApplication* word_application,
		TWordDocument *word_document, vector<TDataSet> &data, int stage, AnsiString &error)
{
	for (int d=0; d<data.size(); d++)
	{
		// header
		SetReportFontBoldUnderlined(word_application, word_document);
		AddCaptionToEndOfDocument(word_application, word_document, "Data set " + AnsiString(d+1), error);

		// Filename
		if (stage == CALIBRATION)
		{
			SetReportFontStandard(word_application, word_document);
			AddCaptionToEndOfDocument(word_application, word_document, "File: " + BaseFileName(CalibrationDataFileNames[d]), error);
		}
		if (stage == VALIDATION)
		{
			SetReportFontStandard(word_application, word_document);
			AddCaptionToEndOfDocument(word_application, word_document, "File: " + BaseFileName(ValidationDataFileName), error);
		}
		if (stage == PREDICTION)
		{
			SetReportFontStandard(word_application, word_document);
			AddCaptionToEndOfDocument(word_application, word_document, "File: " + BaseFileName(PredictionsDataFileName), error);
		}

		// description
		if (stage == CALIBRATION || stage == VALIDATION)
		{
			SetReportFontStandard(word_application, word_document);
			AddCaptionToEndOfDocument(word_application, word_document, "Description (optional): \r\n" + data[d].Description, error);
		}

		AnsiString control_group;
		if (stage == CALIBRATION || stage == VALIDATION)
			if (data[d].ControlGroupIndex >= 0 &&
					data[d].ControlGroupIndex < data[d].Concentration.GroupNames.size())
			{
				control_group = data[d].Concentration.GroupNames[data[d].ControlGroupIndex];
				AddCaptionToEndOfDocument(word_application, word_document, "Control group: '" + control_group + "'", error);
			}

        // survival data
		if (stage == CALIBRATION || stage == VALIDATION)
		{
			SetReportFontTableCaption(word_application, word_document);
			AddCaptionToEndOfDocument(word_application, word_document, "Survival data of input data set " + AnsiString(d+1) + ":", error);
			SetReportFontTable(word_application, word_document);
			AddDataTableToEndOfDocument(word_application, word_document, data[d].Survival);
			AddParagraphToEndOfDocument(word_application, word_document);
		}

		// conc data
		if (stage == CALIBRATION || stage == VALIDATION)
		{
			SetReportFontTableCaption(word_application, word_document);
			AddCaptionToEndOfDocument(word_application, word_document, "Concentration data of input data set " + AnsiString(d+1) + ":", error);
			SetReportFontTable(word_application, word_document);
			AddDataTableToEndOfDocument(word_application, word_document, data[d].Concentration);
			AddParagraphToEndOfDocument(word_application, word_document);
		}
	}

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::AddGoodnessOfFitToEndOfDocument(TWordApplication* word_application,
		TWordDocument *word_document, TGoodnessOfFit &gof, int stage, AnsiString &error)
{
	SetReportFontStandard(word_application, word_document);
	AddCaptionToEndOfDocument(word_application, word_document, "Model efficiency (NSE, r-square): " + gof.R_square, error);
	AddCaptionToEndOfDocument(word_application, word_document, "Normalised root-means-square error (NRMSE): " + gof.NRMSE + " %", error);
	if (stage == CALIBRATION)
	{
		AddCaptionToEndOfDocument(word_application, word_document, "Minus log-likelihood (MLL): " + gof.MLL, error);
		AddCaptionToEndOfDocument(word_application, word_document, "AIC: " + gof.AIC, error);
    }
	AddCaptionToEndOfDocument(word_application, word_document, "Survival probability prediction error (SPPE) for each treatment:", error);

	vector<AnsiString> text = gof.SPPE;
	text.insert(text.begin(), "Data set\tTreatment\tValue");
	AddTextTableToEndOfDocument(word_application, word_document, text);

	return;
}
// ---------------------------------------------------------------------------

bool TMainForm::CreateCalibrationReport(TWordApplication* word_application,
		TWordDocument *word_document, AnsiString &error)
{
	WordRangePtr selection;

	// headline
	SetReportFontCaptionOne(word_application, word_document);
	AddPageBreakToEndOfDocument(word_application, word_document);
	AddCaptionToEndOfDocument(word_application, word_document, "Calibration", error);

	// input data
	SetReportFontCaptionTwo(word_application, word_document);
	AddCaptionToEndOfDocument(word_application, word_document, "Calibration input data", error);
	AddInputDataToReport(word_application, word_document, CalibrationData, CALIBRATION, error);

	// settings
	AddPageBreakToEndOfDocument(word_application, word_document);
	SetReportFontCaptionTwo(word_application, word_document);
	AddCaptionToEndOfDocument(word_application, word_document, "Calibration settings", error);
	SetReportFontStandard(word_application, word_document);
	AddCaptionToEndOfDocument(word_application, word_document, "Calibration parameter settings for GUTS-RED-SD:", error);
	SetReportFontTable(word_application, word_document);
	AddSettingsTableToEndOfDocument(word_application, word_document, PMatSD);

	AddParagraphToEndOfDocument(word_application, word_document);
	SetReportFontStandard(word_application, word_document);
	AddCaptionToEndOfDocument(word_application, word_document, "Calibration parameter settings for GUTS-RED-IT:", error);
	SetReportFontTable(word_application, word_document);
	AddSettingsTableToEndOfDocument(word_application, word_document, PMatIT);

	if (ExpertSettingsOn || PrefitHBToControl || ParameterRangesManipulated)
	{
		AddParagraphToEndOfDocument(word_application, word_document);
		SetReportFontTable(word_application, word_document);
		AddCaptionToEndOfDocument(word_application, word_document, "Note:", error);
	}
	if (ExpertSettingsOn)
	{
		SetReportFontTable(word_application, word_document);
		AddCaptionToEndOfDocument(word_application, word_document, "Expert settings were used (calibration settings may have been changed).", error);
	}
	if (ParameterRangesManipulated)
	{
		SetReportFontTable(word_application, word_document);
		AddCaptionToEndOfDocument(word_application, word_document, "Parameter ranges were changed manually.", error);
	}
	if (PrefitHBToControl)
	{
		SetReportFontTable(word_application, word_document);
		AddCaptionToEndOfDocument(word_application, word_document, "Background hazard (hb) was prefitted to control.", error);
	}

	// results
	AddPageBreakToEndOfDocument(word_application, word_document);
	SetReportFontCaptionTwo(word_application, word_document);
	AddCaptionToEndOfDocument(word_application, word_document, "Calibration results", error);

	if (!CalibrationPerformed)
	{
		SetReportFontStandard(word_application, word_document);
		AddCaptionToEndOfDocument(word_application, word_document, "No calibration performed.", error);
	}
	else
	{
		// SD
		// fitted parameters
		SetReportFontBoldUnderlined(word_application, word_document);
		AddCaptionToEndOfDocument(word_application, word_document, "Fitted parameters for GUTS-RED-SD:", error);
		SetReportFontStandard(word_application, word_document);
		AddParameterValuesAndCI(word_application, word_document, SD, error);
		SetReportFontSmall(word_application, word_document);
		AddCaptionToEndOfDocument(word_application, word_document, "* edge of 95% parameter CI has run into a boundary", error);
		AddCaptionToEndOfDocument(word_application, word_document, "  (this may also affect CIs of other parameters)", error);
		AddParagraphToEndOfDocument(word_application, word_document);

		// Goodness of fit
		SetReportFontBoldUnderlined(word_application, word_document);
		AddCaptionToEndOfDocument(word_application, word_document, "Goodness of fit for calibration data (GUTS-RED-SD):", error);
		AddGoodnessOfFitToEndOfDocument(word_application, word_document, CalibrationSDGoodnessOfFit, CALIBRATION, error);

		// LCx
		AddParagraphToEndOfDocument(word_application, word_document);
		SetReportFontBoldUnderlined(word_application, word_document);
		AddCaptionToEndOfDocument(word_application, word_document, "GUTS-RED-SD results table for LCx,t [" + ConcUnit + "], with 95% CI:", error);
		AddTextTableToEndOfDocument(word_application, word_document, LCxSD_out);

		// Plots
		if (CalibrationResultsPlotted)
		{
			AddPageBreakToEndOfDocument(word_application, word_document);
			SetReportFontBoldUnderlined(word_application, word_document);
			AddCaptionToEndOfDocument(word_application, word_document, "Plots for GUTS-RED-SD calibration:", error);

			SetReportFontFigureCaption(word_application, word_document);
			AddCaptionToEndOfDocument(word_application, word_document, "Parameter space plot for the calibration of GUTS-RED-SD:", error);
			AddFigureToEndOfFile(word_application, word_document, CalibrationSDOutputParamImage, false, imgBITMAP, error);

			TImage* calibration_SD_output_survival_image;
			AddPageBreakToEndOfDocument(word_application, word_document);
			SetReportFontFigureCaption(word_application, word_document);
			AddCaptionToEndOfDocument(word_application, word_document, "Exposure, damage and survival plots for the calibration of GUTS-RED-SD:", error);
			for (int i = 0; i < CalibSDSurvivalScrollBox->ComponentCount; i++)
			{
				calibration_SD_output_survival_image = (TImage*)CalibSDSurvivalScrollBox->Components[i];
				if (i > 0)
				{
					AddPageBreakToEndOfDocument(word_application, word_document);
					AddCaptionToEndOfDocument(word_application, word_document, "... continued plot:", error);
				}
				AddFigureToEndOfFile(word_application, word_document, calibration_SD_output_survival_image, true, imgMETAFILE, error);
			}

			AddPageBreakToEndOfDocument(word_application, word_document);
			SetReportFontFigureCaption(word_application, word_document);
			AddCaptionToEndOfDocument(word_application, word_document, "Observed vs. Predicted survival plot for the calibration of GUTS-RED-SD:", error);
			AddFigureToEndOfFile(word_application, word_document, CalibrationSDOutputObsVSPredSurvivorsImage, false, imgMETAFILE, error);

			//AddParagraphToEndOfDocument(word_application, word_document);
			SetReportFontFigureCaption(word_application, word_document);
			AddCaptionToEndOfDocument(word_application, word_document, "Observed vs. Predicted deaths plot for the calibration of GUTS-RED-SD:", error);
			AddFigureToEndOfFile(word_application, word_document, CalibrationSDOutputObsVSPredDeathsImage, false, imgMETAFILE, error);

			//AddParagraphToEndOfDocument(word_application, word_document);
			SetReportFontFigureCaption(word_application, word_document);
			AddCaptionToEndOfDocument(word_application, word_document, "LCx versus time with confidence intervals (plotted for 16 days, GUTS-RED-SD):", error);
			AddFigureToEndOfFile(word_application, word_document, CalibrationSDOutputLCxImage, false, imgMETAFILE, error);
		}


		// IT
		// fitted parameters
		AddPageBreakToEndOfDocument(word_application, word_document);
		SetReportFontBoldUnderlined(word_application, word_document);
		AddCaptionToEndOfDocument(word_application, word_document, "Fitted parameters for GUTS-RED-IT:", error);
		SetReportFontStandard(word_application, word_document);
		AddParameterValuesAndCI(word_application, word_document, IT, error);
		SetReportFontSmall(word_application, word_document);
		AddCaptionToEndOfDocument(word_application, word_document, "* edge of 95% parameter CI has run into a boundary", error);
		AddCaptionToEndOfDocument(word_application, word_document, "  (this may also affect CIs of other parameters)", error);
        AddParagraphToEndOfDocument(word_application, word_document);

		// Goodness of fit
        SetReportFontBoldUnderlined(word_application, word_document);
		AddCaptionToEndOfDocument(word_application, word_document, "Goodness of fit for calibration data (GUTS-RED-IT):", error);
		AddGoodnessOfFitToEndOfDocument(word_application, word_document, CalibrationITGoodnessOfFit, CALIBRATION, error);

		// LCx
		AddParagraphToEndOfDocument(word_application, word_document);
		SetReportFontBoldUnderlined(word_application, word_document);
		AddCaptionToEndOfDocument(word_application, word_document, "GUTS-RED-IT results table for LCx,t [" + ConcUnit + "], with 95% CI:", error);
		AddTextTableToEndOfDocument(word_application, word_document, LCxIT_out);

		// Plots
		if (CalibrationResultsPlotted)
		{
			AddPageBreakToEndOfDocument(word_application, word_document);
			SetReportFontBoldUnderlined(word_application, word_document);
			AddCaptionToEndOfDocument(word_application, word_document, "Plots for GUTS-RED-IT calibration:", error);

			SetReportFontFigureCaption(word_application, word_document);
			AddCaptionToEndOfDocument(word_application, word_document, "Parameter space plot for the calibration of GUTS-RED-IT:", error);
			AddFigureToEndOfFile(word_application, word_document, CalibrationITOutputParamImage, false, imgBITMAP, error);

			TImage* calibration_IT_output_survival_image;
			AddPageBreakToEndOfDocument(word_application, word_document);
			SetReportFontFigureCaption(word_application, word_document);
			AddCaptionToEndOfDocument(word_application, word_document, "Exposure, damage and survival plots for the calibration of GUTS-RED-IT:", error);
			for (int i = 0; i < CalibITSurvivalScrollBox->ComponentCount; i++)
			{
				calibration_IT_output_survival_image = (TImage*)CalibITSurvivalScrollBox->Components[i];
				if (i > 0)
				{
					AddPageBreakToEndOfDocument(word_application, word_document);
					AddCaptionToEndOfDocument(word_application, word_document, "... continued plot:", error);
				}
				AddFigureToEndOfFile(word_application, word_document, calibration_IT_output_survival_image, true, imgMETAFILE, error);
			}

			AddPageBreakToEndOfDocument(word_application, word_document);
			SetReportFontFigureCaption(word_application, word_document);
			AddCaptionToEndOfDocument(word_application, word_document, "Observed vs. Predicted survival plot for the calibration of GUTS-RED-IT:", error);
			AddFigureToEndOfFile(word_application, word_document, CalibrationITOutputObsVSPredSurvivorsImage, false, imgMETAFILE, error);

			//AddParagraphToEndOfDocument(word_application, word_document);
			SetReportFontFigureCaption(word_application, word_document);
			AddCaptionToEndOfDocument(word_application, word_document, "Observed vs. Predicted deaths plot for the calibration of GUTS-RED-IT:", error);
			AddFigureToEndOfFile(word_application, word_document, CalibrationITOutputObsVSPredDeathsImage, false, imgMETAFILE, error);

			//AddParagraphToEndOfDocument(word_application, word_document);
			SetReportFontFigureCaption(word_application, word_document);
			AddCaptionToEndOfDocument(word_application, word_document, "LCx versus time with confidence intervals (plotted for 16 days, GUTS-RED-IT):", error);
			AddFigureToEndOfFile(word_application, word_document, CalibrationITOutputLCxImage, false, imgMETAFILE, error);
		}
	}

	return true;
}
// ---------------------------------------------------------------------------

bool TMainForm::CreateValidationReport(TWordApplication* word_application,
		TWordDocument *word_document, AnsiString &error)
{
	WordRangePtr selection;

	// caption
	SetReportFontCaptionOne(word_application, word_document);
	AddPageBreakToEndOfDocument(word_application, word_document);
	AddCaptionToEndOfDocument(word_application, word_document, "Validation", error);

	if (!SDValidationPerformed && !ITValidationPerformed)
	{
		AddParagraphToEndOfDocument(word_application, word_document);
		SetReportFontStandard(word_application, word_document);
		AddCaptionToEndOfDocument(word_application, word_document, "No validation performed!", error);

		return true;
	}

	// input data
	AddInputDataToReport(word_application, word_document, ValidationData, VALIDATION, error);
	AddParagraphToEndOfDocument(word_application, word_document);

	// results
	SetReportFontStandard(word_application, word_document);
	// SD
	if (SDValidationPerformed)
	{
		AddPageBreakToEndOfDocument(word_application, word_document);

		// Goodness of fit
		SetReportFontBoldUnderlined(word_application, word_document);
		AddCaptionToEndOfDocument(word_application, word_document, "Goodness of fit for validation data (GUTS-RED-SD):", error);
		AddGoodnessOfFitToEndOfDocument(word_application, word_document, ValidationSDGoodnessOfFit, VALIDATION, error);

		// Plots
		if (ValidationResultsPlotted)
		{
			if (ValidationSDOutputSurvivalScrollBox->ComponentCount > 0)
			{
				TImage* validation_SD_output_survival_image;
				AddPageBreakToEndOfDocument(word_application, word_document);
                SetReportFontFigureCaption(word_application, word_document);
				AddCaptionToEndOfDocument(word_application, word_document, "Exposure, damage and survival plot for the validation of GUTS_RED_SD:", error);
				for (int i = 0; i < ValidationSDOutputSurvivalScrollBox->ComponentCount; i++)
				{
					validation_SD_output_survival_image = (TImage*)ValidationSDOutputSurvivalScrollBox->Components[i];
					if (i > 0)
					{
                        AddPageBreakToEndOfDocument(word_application, word_document);
						AddCaptionToEndOfDocument(word_application, word_document, "... continued plot:", error);
					}
					AddFigureToEndOfFile(word_application, word_document, validation_SD_output_survival_image, true, imgMETAFILE, error);
				}

				AddPageBreakToEndOfDocument(word_application, word_document);
				SetReportFontFigureCaption(word_application, word_document);
				AddCaptionToEndOfDocument(word_application, word_document, "Observed vs. Predicted survival plot for the validation of GUTS_RED_SD:", error);
				AddFigureToEndOfFile(word_application, word_document, ValidationSDOutputObsVSPredSurvivorsImage, false, imgMETAFILE, error);

				//AddParagraphToEndOfDocument(word_application, word_document);
				SetReportFontFigureCaption(word_application, word_document);
				AddCaptionToEndOfDocument(word_application, word_document, "Observed vs. Predicted deaths plot for the validation of GUTS_RED_SD:", error);
				AddFigureToEndOfFile(word_application, word_document, ValidationSDOutputObsVSPredDeathsImage, false, imgMETAFILE, error);
			}
		}
	}

	// results
	// IT
	if (ITValidationPerformed)
	{
		AddPageBreakToEndOfDocument(word_application, word_document);

		// Goodness of fit
		SetReportFontBoldUnderlined(word_application, word_document);
		AddCaptionToEndOfDocument(word_application, word_document, "Goodness of fit for validation data (GUTS-RED-IT):", error);
		AddGoodnessOfFitToEndOfDocument(word_application, word_document, ValidationITGoodnessOfFit, VALIDATION, error);

		// Plots
		if (ValidationResultsPlotted)
		{
			if (ValidationITOutputSurvivalScrollBox->ComponentCount > 0)
			{
				TImage* validation_IT_output_survival_image;
				AddPageBreakToEndOfDocument(word_application, word_document);
				SetReportFontFigureCaption(word_application, word_document);
				AddCaptionToEndOfDocument(word_application, word_document, "Exposure, damage and survival plot for the validation of GUTS_RED_IT:", error);
				for (int i = 0; i < ValidationITOutputSurvivalScrollBox->ComponentCount; i++)
				{
					validation_IT_output_survival_image = (TImage*)ValidationITOutputSurvivalScrollBox->Components[i];
					if (i > 0)
					{
						AddPageBreakToEndOfDocument(word_application, word_document);
						AddCaptionToEndOfDocument(word_application, word_document, "... continued plot:", error);
					}
					AddFigureToEndOfFile(word_application, word_document, validation_IT_output_survival_image, true, imgMETAFILE, error);
				}

				AddPageBreakToEndOfDocument(word_application, word_document);
				SetReportFontFigureCaption(word_application, word_document);
				AddCaptionToEndOfDocument(word_application, word_document, "Observed vs. Predicted survival plot for the validation of GUTS_RED_IT:", error);
				AddFigureToEndOfFile(word_application, word_document, ValidationITOutputObsVSPredSurvivorsImage, false, imgMETAFILE, error);

				//AddParagraphToEndOfDocument(word_application, word_document);
				SetReportFontFigureCaption(word_application, word_document);
				AddCaptionToEndOfDocument(word_application, word_document, "Observed vs. Predicted deaths plot for the validation of GUTS_RED_IT:", error);
				AddFigureToEndOfFile(word_application, word_document, ValidationITOutputObsVSPredDeathsImage, false, imgMETAFILE, error);
			}
		}
	}

	return true;
}
// ---------------------------------------------------------------------------

bool TMainForm::CreatePredictionReport(TWordApplication* word_application,
		TWordDocument *word_document, AnsiString &error)
{
	WordRangePtr selection;

	// caption
	SetReportFontCaptionOne(word_application, word_document);
	AddPageBreakToEndOfDocument(word_application, word_document);
	AddCaptionToEndOfDocument(word_application, word_document, "Predictions", error);

	if (!SDPredictionsPerformed && !ITPredictionsPerformed)
	{
		AddParagraphToEndOfDocument(word_application, word_document);
		SetReportFontStandard(word_application, word_document);
		AddCaptionToEndOfDocument(word_application, word_document, "No predictions performed!", error);

		return true;
	}

	// input data
	AddInputDataToReport(word_application, word_document, PredictionsData, PREDICTION, error);


	if (SDPredictionsPerformed)
	{
		// LPx data
		AddPageBreakToEndOfDocument(word_application, word_document);
		SetReportFontBoldUnderlined(word_application, word_document);
		AddCaptionToEndOfDocument(word_application, word_document, "GUTS-RED-SD results table for LPx [-]:", error);
		AddTextTableToEndOfDocument(word_application, word_document, LPxSD_out);
		AddParagraphToEndOfDocument(word_application, word_document);
		SetReportFontFigureCaption(word_application, word_document);
		AddCaptionToEndOfDocument(word_application, word_document, "Survival at the end of the profile versus multiplication factor for the exposure profile (GUTS-RED-SD):", error);
		AddFigureToEndOfFile(word_application, word_document, PredictionsSDOutputMultiplicationFactorImage, true, imgMETAFILE, error);

		// exposure, damage, survival
		AddPageBreakToEndOfDocument(word_application, word_document);
		TImage* prediction_SD_output_survival_image;
        SetReportFontFigureCaption(word_application, word_document);
		AddCaptionToEndOfDocument(word_application, word_document, "Exposure, damage and survival plot for the predictions of GUTS-RED-SD (MF = multiplication factor):", error);
		for (int i = 0; i < PredictionsSDSurvivalScrollBox->ComponentCount; i++)
		{
			prediction_SD_output_survival_image = (TImage*)PredictionsSDSurvivalScrollBox->Components[i];
			AddFigureToEndOfFile(word_application, word_document, prediction_SD_output_survival_image, true, imgMETAFILE, error);
		}
	}

	if (ITPredictionsPerformed)
	{
		// LPx
		AddPageBreakToEndOfDocument(word_application, word_document);
		SetReportFontBoldUnderlined(word_application, word_document);
		AddCaptionToEndOfDocument(word_application, word_document, "GUTS-RED-IT results table for LPx [-]:", error);
		AddTextTableToEndOfDocument(word_application, word_document, LPxIT_out);
		AddParagraphToEndOfDocument(word_application, word_document);
		SetReportFontFigureCaption(word_application, word_document);
		AddCaptionToEndOfDocument(word_application, word_document, "Survival at the end of the profile versus multiplication factor for the exposure profile (GUTS-RED-IT):", error);
		AddFigureToEndOfFile(word_application, word_document, PredictionsITOutputMultiplicationFactorImage, true, imgMETAFILE, error);

		// exposure, damage, survival
		AddPageBreakToEndOfDocument(word_application, word_document);
		TImage* prediction_IT_output_survival_image;
		SetReportFontFigureCaption(word_application, word_document);
		AddCaptionToEndOfDocument(word_application, word_document, "Exposure, damage and survival plot for the predictions of GUTS-RED-IT (MF = multiplication factor):", error);
		for (int i = 0; i < PredictionsITSurvivalScrollBox->ComponentCount; i++)
		{
			prediction_IT_output_survival_image = (TImage*)PredictionsITSurvivalScrollBox->Components[i];
			AddFigureToEndOfFile(word_application, word_document, prediction_IT_output_survival_image, true, imgMETAFILE, error);
		}
	}

	return true;
}
// ---------------------------------------------------------------------------

bool TMainForm::CreateBatchPredictionReport(TWordApplication* word_application,
		TWordDocument *word_document, AnsiString &error)
{
	WordRangePtr selection;

	// caption
	SetReportFontCaptionOne(word_application, word_document);
	AddPageBreakToEndOfDocument(word_application, word_document);
	AddCaptionToEndOfDocument(word_application, word_document, "Batch predictions", error);

	SetReportFontBoldUnderlined(word_application, word_document);
	AddCaptionToEndOfDocument(word_application, word_document, "GUTS-RED results table for LPx [-]:", error);
    SetReportFontStandard(word_application, word_document);
    for (
			vector<AnsiString >::const_iterator cit = LPxBatch_Samplesize_Info_out.begin();
			cit != LPxBatch_Samplesize_Info_out.end();
			++cit
		)
	{
        AddCaptionToEndOfDocument(word_application, word_document, *cit, error);
	}
	AddTextTableToEndOfDocument(word_application, word_document, LPxBatch_out);

	return true;
}
// ---------------------------------------------------------------------------

bool TMainForm::OpenWordApplicationAndSaveDocument(TWordApplication* word_application,
		TWordDocument *word_document, string filename, AnsiString &error)
{
	try
	{
		word_application->set_Visible(true);

		AnsiString empty = "";
		word_application->Documents->Open(OleVariant(StringToOleStr((UnicodeString)filename.c_str())),
			// FileName : der Name der zu ladenden Datei mit Pfad!
			OleVariant(false),
			// ConfirmConversions : ob die Datei konvertiert werden soll/muß
			OleVariant(false),
			// ReadOnly : ob die Datei schreibgeschütz geöffnet werden soll
			OleVariant(false),
			// AddToRecentFiles : ob die Datei in die Liste der letzten geöffneten Dateien angefügt werden soll
			OleVariant(StringToOleStr(empty)),
			// PasswordDocument : Kennwort zum Öffnen des Dokuments
			OleVariant(StringToOleStr(empty)),
			// PasswordTemplate : Kennwort zum Öffnen der Vorlage
			OleVariant(false),
			// Revert : ob Änderungen verworfen werden und Datei neu geöffnet werden soll, wenn Datei bereits geöffnet ist
			OleVariant(StringToOleStr(empty)),
			// WritePasswordDocument : Kennwort zum Speichern des Dokuments
			OleVariant(StringToOleStr(empty)),
			// WritePasswordTemplate : Kennwort zum Speichern der Vorlage
			OleVariant(L"wdOpenFormatAuto"),
			// Format : Das Dateikonvertierungsprogramm zum Öffnen
			OleVariant(),
			// Encoding : Das zu verwendende Dokumentcodierung zum Öffnen
			OleVariant(true)
			// Visible : ob das Dokument in einem sichtbaren Fenster geöffnet werden soll
			);
	}
	catch (...)
	{
		Screen->Cursor = crDefault;
		error += "Could not access MS Word!";
		return false;
	}
	if (!word_application)
	{
		Screen->Cursor = crDefault;
		error += "Could not access MS Word! Please switch to MS Word or try again later.";
		return false;
	}

	word_document->Save();
	return true;
}
// ---------------------------------------------------------------------------

bool TMainForm::SaveDocumentAndCloseWordApplication(TWordApplication* wordapplication,
		TWordDocument *word_document, AnsiString &error)
{
	try
	{
		word_document->Save();
		wordapplication->set_Visible(true);
		delete wordapplication;
	}
	catch (...)
	{
		error += "unable to close word properly.";
		return false;
	}
	return true;
}
// ---------------------------------------------------------------------------

bool TMainForm::PrepareDocument(TWordApplication* wordapplication, AnsiString &error)
{
	try
	{
		wordapplication->CaptionLabels->Add(L"Graph");
		wordapplication->CaptionLabels->Add(L"Table A");
	}
	catch (...)
	{
		return false;
	}
	return true;
}
// ---------------------------------------------------------------------------

bool TMainForm::GetEndOfDocument(TWordApplication* wordapplication,
		TWordDocument *word_document, WordRangePtr &end_of_document)
{
	int no_paragraphs;
	end_of_document = wordapplication->Selection->Range;
	no_paragraphs = word_document->Paragraphs->Count;
	Paragraph* test = word_document->Paragraphs->Item(no_paragraphs);

	end_of_document->Start = test->Range->End;
	end_of_document->End = test->Range->End;
	return true;
}

// ---------------------------------------------------------------------------

void TMainForm::SetReportFontTitle(TWordApplication* word_application,
		TWordDocument *word_document)
{
	WordRangePtr end_of_document;
	WordRangePtr range_of_inserted_caption;
	GetEndOfDocument(word_application, word_document, end_of_document);

	end_of_document->Font->Size = 18;
	end_of_document->Font->set_Name(L"Arial");
	end_of_document->Font->Bold = true;
	end_of_document->Font->Underline = 0;

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::SetReportFontStandard(TWordApplication* wordapplication,
		TWordDocument *word_document)
{
	WordRangePtr end_of_document;
	WordRangePtr range_of_inserted_caption;
	GetEndOfDocument(wordapplication, word_document, end_of_document);

	end_of_document->Font->Size = 10;
	end_of_document->Font->set_Name(L"Arial");
	end_of_document->Font->Bold = false;
	end_of_document->Font->Underline = 0;

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::SetReportFontTableCaption(TWordApplication* wordapplication,
		TWordDocument *word_document)
{
	WordRangePtr end_of_document;
	WordRangePtr range_of_inserted_caption;
	GetEndOfDocument(wordapplication, word_document, end_of_document);

	end_of_document->Font->Size = 11;
	end_of_document->Font->set_Name(L"Arial");
	end_of_document->Font->Bold = true;
	end_of_document->Font->Underline = 0;

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::SetReportFontFigureCaption(TWordApplication* wordapplication,
		TWordDocument *word_document)
{
	WordRangePtr end_of_document;
	WordRangePtr range_of_inserted_caption;
	GetEndOfDocument(wordapplication, word_document, end_of_document);

	end_of_document->Font->Size = 11;
	end_of_document->Font->set_Name(L"Arial");
	end_of_document->Font->Bold = true;
	end_of_document->Font->Underline = 0;

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::SetReportFontTable(TWordApplication* wordapplication,
		TWordDocument *word_document)
{
	WordRangePtr end_of_document;
	WordRangePtr range_of_inserted_caption;
	GetEndOfDocument(wordapplication, word_document, end_of_document);

	end_of_document->Font->Size = 9;
	end_of_document->Font->set_Name(L"Arial");
	end_of_document->Font->Bold = false;
	end_of_document->Font->Underline = 0;

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::SetReportFontSmall(TWordApplication* wordapplication,
		TWordDocument *word_document)
{
	WordRangePtr end_of_document;
	WordRangePtr range_of_inserted_caption;
	GetEndOfDocument(wordapplication, word_document, end_of_document);

	end_of_document->Font->Size = 8;
	end_of_document->Font->set_Name(L"Arial");
	end_of_document->Font->Bold = false;
	end_of_document->Font->Underline = 0;

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::SetReportFontCaptionOne(TWordApplication* wordapplication,
		TWordDocument *word_document)
{
	WordRangePtr end_of_document;
	WordRangePtr range_of_inserted_caption;
	GetEndOfDocument(wordapplication, word_document, end_of_document);

	end_of_document->Font->Size = 16;
	end_of_document->Font->set_Name(L"Arial");
	end_of_document->Font->Bold = true;
	end_of_document->Font->Underline = 0;

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::SetReportFontCaptionTwo(TWordApplication* wordapplication,
		TWordDocument *word_document)
{
	WordRangePtr end_of_document;
	WordRangePtr range_of_inserted_caption;
	GetEndOfDocument(wordapplication, word_document, end_of_document);

	end_of_document->Font->Size = 12;
	end_of_document->Font->set_Name(L"Arial");
	end_of_document->Font->Bold = true;
	end_of_document->Font->Underline = 0;

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::SetReportFontBoldUnderlined(TWordApplication* wordapplication,
		TWordDocument *word_document)
{
	WordRangePtr end_of_document;
	WordRangePtr range_of_inserted_caption;
	GetEndOfDocument(wordapplication, word_document, end_of_document);

	end_of_document->Font->Size = 10;
	end_of_document->Font->set_Name(L"Arial");
	end_of_document->Font->Bold = true;
	end_of_document->Font->Underline = 1;

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::AddSettingsTableToEndOfDocument(TWordApplication* wordapplication,
		TWordDocument *word_document, TParameterMatrix &pmat)
{
	WordRangePtr end_of_document;
	WordRangePtr range_of_inserted_caption;
	GetEndOfDocument(wordapplication, word_document, end_of_document);

	wordapplication->Selection->Range->Start = end_of_document->Start;
	wordapplication->Selection->Range->End = end_of_document->End;

	int cols, rows;
	rows = 6;
	cols = 5;

	Table* mCurrentTable;
	mCurrentTable = word_document->Tables->Add(end_of_document, rows, cols);//wordapplication->Selection->Range,rows,cols);
	mCurrentTable->AutoFormat(OleVariant(37));    // 16 = standard grid, 37 = "professional"

	// table headers
	mCurrentTable->Cell(1, 1)->Select();
	wordapplication->Selection->set_Text(StringToOleStr(AnsiString("Parameter")));
	mCurrentTable->Cell(1, 2)->Select();
	wordapplication->Selection->set_Text(StringToOleStr(AnsiString("Fit")));
	mCurrentTable->Cell(1, 3)->Select();
	wordapplication->Selection->set_Text(StringToOleStr(AnsiString("Min")));
	mCurrentTable->Cell(1, 4)->Select();
	wordapplication->Selection->set_Text(StringToOleStr(AnsiString("Max")));
	mCurrentTable->Cell(1, 5)->Select();
	wordapplication->Selection->set_Text(StringToOleStr(AnsiString("Scale")));

	// data
	// GUTS parameter names
	for (int r=1; r<rows; r++)
	{
		mCurrentTable->Cell(r+1, 1)->Select();
		wordapplication->Selection->set_Text(StringToOleStr(AnsiString(gutsPARAMETERS[r-1].c_str())));
	}
	// fit yes/no?
	for (int r=1; r<rows; r++)
	{
		mCurrentTable->Cell(r+1, 2)->Select();
		wordapplication->Selection->set_Text(StringToOleStr(AnsiString(pmat.Fit[r-1] == gutsFIT_PARAM ? "Yes" : "No")));
	}
	// parameter minimum
	for (int r=1; r<rows; r++)
	{
		mCurrentTable->Cell(r+1, 3)->Select();
		if (pmat.Min[r-1] == -1)
			wordapplication->Selection->set_Text(StringToOleStr(AnsiString("Inf")));
		else
			wordapplication->Selection->set_Text(StringToOleStr(MyFloatToStr(pmat.Min[r-1], 4)));
	}
	// parameter maximum
	for (int r=1; r<rows; r++)
	{
		mCurrentTable->Cell(r+1, 4)->Select();
		if (pmat.Max[r-1] == -1)
			wordapplication->Selection->set_Text(StringToOleStr(AnsiString("Inf")));
		else
			wordapplication->Selection->set_Text(StringToOleStr(MyFloatToStr(pmat.Max[r-1], 4)));
	}
	// scale Log/Norm?
	for (int r=1; r<rows; r++)
	{
		mCurrentTable->Cell(r+1, 5)->Select();
		wordapplication->Selection->set_Text(StringToOleStr(AnsiString(pmat.Scale[r-1] == gutsNORM_SCALE_PARAM ? "Norm" : "Log")));
	}

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::AddTextTableToEndOfDocument(TWordApplication* wordapplication,
		TWordDocument *word_document, vector<AnsiString> &text)
{
	if (text.size() < 1)
        return;

	WordRangePtr end_of_document;
	WordRangePtr range_of_inserted_caption;
	GetEndOfDocument(wordapplication, word_document, end_of_document);

	wordapplication->Selection->Range->Start = end_of_document->Start;
	wordapplication->Selection->Range->End = end_of_document->End;

	SetReportFontTable(wordapplication, word_document);

	string s;
	vector<string> sv;

	s = text[0].c_str();
	sv = SplitString(s, "\t");

	int rows = text.size();
	int cols = sv.size();

	Table* mCurrentTable;
	mCurrentTable = word_document->Tables->Add(end_of_document, rows, cols);
	mCurrentTable->AutoFormat(OleVariant(37));    // 16 = standard grid, 37 = "professional"

	for (int r=1; r<=rows; r++)
	{
		sv = SplitString(text[r-1].c_str(), "\t");
		for (int c=1; c<=cols; c++)
		{
			mCurrentTable->Cell(r, c)->Select();
			wordapplication->Selection->set_Text(StringToOleStr(AnsiString(sv[c-1].c_str())));
		}
	}

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::AddDataTableToEndOfDocument(TWordApplication* wordapplication,
		TWordDocument *word_document, TGridData &data)
{
	WordRangePtr end_of_document;
	WordRangePtr range_of_inserted_caption;
	GetEndOfDocument(wordapplication, word_document, end_of_document);

	wordapplication->Selection->Range->Start = end_of_document->Start;
	wordapplication->Selection->Range->End = end_of_document->End;


	int cols, rows;
	rows = data.Times.size() + 1;
	cols = data.GroupNames.size() + 1;

	Table* mCurrentTable;
	mCurrentTable = word_document->Tables->Add(end_of_document, rows, cols);
	mCurrentTable->AutoFormat(OleVariant(37));    // 16 = standard grid

	mCurrentTable->Cell(1, 1)->Select();
	wordapplication->Selection->set_Text(StringToOleStr(AnsiString("Time [d]")));

	// table headers
	for (int c=1; c<cols; c++)
	{
		mCurrentTable->Cell(1, c+1)->Select();
		wordapplication->Selection->set_Text(StringToOleStr(data.GroupNames[c-1]));
	}

	// time column
	for (int r=1; r<rows; r++)
	{
		mCurrentTable->Cell(r+1, 1)->Select();
		wordapplication->Selection->set_Text(StringToOleStr(AnsiString(data.Times[r-1])));
	}

	// data
	for (int c=1; c<cols; c++)
		for (int r=1; r<rows; r++)
		{
			mCurrentTable->Cell(r+1, c+1)->Select();
			wordapplication->Selection->set_Text(StringToOleStr(AnsiString(data.Values[c-1][r-1])));
		}

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::AddTableCaption(TWordApplication* wordapplication,
		TWordDocument *word_document, AnsiString caption)
{
	WordRangePtr end_of_document;
	WordRangePtr range_of_inserted_text;
	GetEndOfDocument(wordapplication, word_document, end_of_document);

	range_of_inserted_text = wordapplication->Selection->Range;
	range_of_inserted_text->End = end_of_document->End;
	range_of_inserted_text->Start = end_of_document->Start;

	end_of_document->InsertCaption(OleVariant(StringToOleStr(AnsiString("Table"))),
		OleVariant(StringToOleStr(caption)), OleVariant(), OleVariant(1));
	range_of_inserted_text->End = end_of_document->End;
	end_of_document->InsertParagraphAfter();

	return;
}
// ---------------------------------------------------------------------------

WordRangePtr TMainForm::AddCaptionToEndOfDocument(TWordApplication* wordapplication,
	TWordDocument *word_document, AnsiString caption, AnsiString &error)
{
	WordRangePtr end_of_document;
	WordRangePtr range_of_inserted_caption;
	GetEndOfDocument(wordapplication, word_document, end_of_document);
	range_of_inserted_caption = wordapplication->Selection->Range;
	range_of_inserted_caption->End = end_of_document->End;
	range_of_inserted_caption->Start = end_of_document->Start;
	end_of_document->set_Text(StringToOleStr(caption));
	range_of_inserted_caption->End = range_of_inserted_caption->Start + caption.Length();
	end_of_document->InsertParagraphAfter();

	return range_of_inserted_caption;
}
// ---------------------------------------------------------------------------

void TMainForm::AddParagraphToEndOfDocument(TWordApplication* wordapplication,
		TWordDocument *word_document)
{
	WordRangePtr end_of_document;
	GetEndOfDocument(wordapplication, word_document, end_of_document);
	end_of_document->InsertParagraphAfter();

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::AddPageBreakToEndOfDocument(TWordApplication* wordapplication,
		TWordDocument *word_document)
{
	WordRangePtr end_of_document;
	GetEndOfDocument(wordapplication, word_document, end_of_document);
	end_of_document->InsertBreak(OleVariant(7));      	// wdPageBreak 	7 	"page break"

	return;
}
// ---------------------------------------------------------------------------

WordRangePtr TMainForm::AddFigureToEndOfFile(TWordApplication* word_application,
		TWordDocument *word_document, TImage* this_image, bool multi_row,
		int image_picture_type, AnsiString &error)
{
	WordRangePtr end_of_document;
	WordRangePtr range_of_inserted_figure = word_application->Selection->Range;

	int plot_width = this_image->Width;
	if (!multi_row)
	{
		GetEndOfDocument(word_application, word_document, end_of_document);
		end_of_document->Select();

		// convert metafile to bitmap for report
		TBitmap *bitmap = new TBitmap;
		if (image_picture_type == imgMETAFILE)
		{
			bitmap->SetSize(this_image->Picture->Metafile->Width, this_image->Picture->Metafile->Height);
			bitmap->Canvas->Draw(0, 0, this_image->Picture->Metafile);
		}
		else
		{
			bitmap->Assign(this_image->Picture->Bitmap);
		}

		Clipboard()->Assign(bitmap);
		word_application->Selection->Paste();
		range_of_inserted_figure->End = word_application->Selection->Range->End;
		end_of_document->InsertParagraphAfter();

		delete bitmap;
	}
	else
	{
		int row_count = 0;
		while (plot_width > 0)  // while there is plot area to plot
		{
			TMetafile *mf = new TMetafile();
			mf->SetSize(MAX_NO_IMAGES_PER_ROW * plotSINGLE_CHART_WIDTH, this_image->Height);
			TMetafileCanvas *mf_canvas = new TMetafileCanvas(mf, 0);

			mf_canvas->Draw(-row_count * MAX_NO_IMAGES_PER_ROW * plotSINGLE_CHART_WIDTH, 0, this_image->Picture->Metafile);
			row_count++;

			delete mf_canvas;

			if (row_count > 1)
			{
				AddPageBreakToEndOfDocument(word_application, word_document);
				AddCaptionToEndOfDocument(word_application, word_document, "... continued plot:", error);
			}

			GetEndOfDocument(word_application, word_document, end_of_document);
			end_of_document->Select();

			// convert metafile to bitmap for report
			TBitmap *bitmap = new TBitmap;
			bitmap->SetSize(mf->Width, mf->Height);
			bitmap->Canvas->Draw(0, 0, mf);

			Clipboard()->Assign(bitmap);
			word_application->Selection->Paste();
			range_of_inserted_figure->End = word_application->Selection->Range->End;
			end_of_document->InsertParagraphAfter();

			delete bitmap;
			delete mf;

			plot_width -= MAX_NO_IMAGES_PER_ROW * plotSINGLE_CHART_WIDTH;
		}
	}

	return range_of_inserted_figure;
}
// ---------------------------------------------------------------------------

WordRangePtr TMainForm::AddFigureToEndOfFile(TWordApplication* wordapplication,
		TWordDocument* word_document, TChart* this_chart, AnsiString &error)
{
	WordRangePtr end_of_document;
	WordRangePtr range_of_inserted_figure = wordapplication->Selection->Range;
	GetEndOfDocument(wordapplication, word_document, end_of_document);
	end_of_document->Select();
	this_chart->CopyToClipboardMetafile(true);
	wordapplication->Selection->Paste();
	range_of_inserted_figure->End = wordapplication->Selection->Range->End;
	range_of_inserted_figure->Start = wordapplication->Selection->Range->Start;
	end_of_document->InsertParagraphAfter();
	return range_of_inserted_figure;
}
// ---------------------------------------------------------------------------

WordRangePtr TMainForm::AddTextFromMemoToEndOfDocument(TWordApplication* wordapplication,
		TWordDocument *word_document, TMemo* this_memo, AnsiString &error)
{
	WordRangePtr end_of_document;
	WordRangePtr range_of_inserted_text;
	GetEndOfDocument(wordapplication, word_document, end_of_document);
	range_of_inserted_text = wordapplication->Selection->Range;
	range_of_inserted_text->End = end_of_document->End;
	range_of_inserted_text->Start = end_of_document->Start;
	for (int i = 0; i < this_memo->Lines->Count; i++)
	{
		end_of_document->set_Text(StringToOleStr(this_memo->Lines->Strings[i]));
		end_of_document->InsertParagraphAfter();
		GetEndOfDocument(wordapplication, word_document, end_of_document);
	}
	range_of_inserted_text->End = end_of_document->End;
	end_of_document->InsertParagraphAfter();
	return range_of_inserted_text;
}
// ---------------------------------------------------------------------------

WordRangePtr TMainForm::AddTextFromStringVecToEndOfDocument(TWordApplication* wordapplication,
	TWordDocument* word_document, vector<AnsiString>memo_strings, AnsiString &error)
{
	WordRangePtr end_of_document;
	WordRangePtr range_of_inserted_text;
	GetEndOfDocument(wordapplication, word_document, end_of_document);
	range_of_inserted_text = wordapplication->Selection->Range;
	range_of_inserted_text->End = end_of_document->End;
	range_of_inserted_text->Start = end_of_document->Start;
	for (int i = 0; i < memo_strings.size(); i++)
	{
		end_of_document->set_Text(StringToOleStr(memo_strings[i]));
		end_of_document->InsertParagraphAfter();
		GetEndOfDocument(wordapplication, word_document, end_of_document);
	}
	range_of_inserted_text->End = end_of_document->End;
	end_of_document->InsertParagraphAfter();
	return range_of_inserted_text;
}
// ---------------------------------------------------------------------------

WordRangePtr TMainForm::AddFigureCaption(TWordApplication* wordapplication,
		TWordDocument *word_document, AnsiString label, AnsiString &error)
{
	WordRangePtr end_of_document;
	WordRangePtr range_of_inserted_text;
	GetEndOfDocument(wordapplication, word_document, end_of_document);
	range_of_inserted_text = wordapplication->Selection->Range;
	range_of_inserted_text->End = end_of_document->End;
	range_of_inserted_text->Start = end_of_document->Start;
	end_of_document->InsertCaption(OleVariant(StringToOleStr(AnsiString("Figure"))), OleVariant(StringToOleStr(label)),
		OleVariant(), OleVariant(1));
	range_of_inserted_text->End = end_of_document->End;
	end_of_document->InsertParagraphAfter();
	return range_of_inserted_text;
}
// ---------------------------------------------------------------------------

WordRangePtr TMainForm::AddParameterValuesAndCI(TWordApplication* wordapplication,
		TWordDocument *word_document, int SDIT, AnsiString &error)
{
	WordRangePtr end_of_document;
	WordRangePtr range_of_inserted_text;
	GetEndOfDocument(wordapplication, word_document, end_of_document);
	range_of_inserted_text = wordapplication->Selection->Range;
	range_of_inserted_text->End = end_of_document->End;
	range_of_inserted_text->Start = end_of_document->Start;

	end_of_document->set_Text(StringToOleStr(AnsiString("Best fit parameter values and their 95% CI")));
	end_of_document->InsertParagraphAfter();
	GetEndOfDocument(wordapplication, word_document, end_of_document);

	AnsiString param = "";
	if (SDIT == SD)
	{
		if (PMatSD.Fit[0])
		{
			param = "kd:\t" + MyFloatToStr(PMatPrintSD.BestValue[0], 4) + "\t(" + MyFloatToStr(PMatPrintSD.LowerCI[0], 4) +
				AnsiString(PMatPrintSD.LowerCIWarning[0].c_str()) + " - " + MyFloatToStr(PMatPrintSD.UpperCI[0], 4) +
				AnsiString(PMatPrintSD.UpperCIWarning[0].c_str()) + ")";
			end_of_document->set_Text(StringToOleStr(param));
			end_of_document->InsertParagraphAfter();
			GetEndOfDocument(wordapplication, word_document, end_of_document);
		}

		if (PMatSD.Fit[1])
		{
			param = "mw:\t" + MyFloatToStr(PMatPrintSD.BestValue[1], 4) + "\t(" + MyFloatToStr(PMatPrintSD.LowerCI[1], 4) +
				AnsiString(PMatPrintSD.LowerCIWarning[1].c_str()) + " - " + MyFloatToStr(PMatPrintSD.UpperCI[1], 4) +
				AnsiString(PMatPrintSD.UpperCIWarning[1].c_str()) + ")";
			end_of_document->set_Text(StringToOleStr(param));
			end_of_document->InsertParagraphAfter();
			GetEndOfDocument(wordapplication, word_document, end_of_document);
		}

		if (PMatSD.Fit[2])
		{
			param = "hb:\t" + MyFloatToStr(PMatPrintSD.BestValue[2], 4) + "\t(" + MyFloatToStr(PMatPrintSD.LowerCI[2], 4) +
				AnsiString(PMatPrintSD.LowerCIWarning[2].c_str()) + " - " + MyFloatToStr(PMatPrintSD.UpperCI[2], 4) +
				AnsiString(PMatPrintSD.UpperCIWarning[2].c_str()) + ")";
			end_of_document->set_Text(StringToOleStr(param));
			end_of_document->InsertParagraphAfter();
			GetEndOfDocument(wordapplication, word_document, end_of_document);
		}

		if (PMatSD.Fit[3])
		{
			param = "bw:\t" + MyFloatToStr(PMatPrintSD.BestValue[3], 4) + "\t(" + MyFloatToStr(PMatPrintSD.LowerCI[3], 4) +
				AnsiString(PMatPrintSD.LowerCIWarning[3].c_str()) + " - " + MyFloatToStr(PMatPrintSD.UpperCI[3], 4) +
				AnsiString(PMatPrintSD.UpperCIWarning[3].c_str()) + ")";
			end_of_document->set_Text(StringToOleStr(param));
			end_of_document->InsertParagraphAfter();
			GetEndOfDocument(wordapplication, word_document, end_of_document);
		}

	}
	else if (SDIT == IT)
	{
		if (PMatIT.Fit[0])
		{
			param = "kd:\t" + MyFloatToStr(PMatPrintIT.BestValue[0], 4) + "\t(" + MyFloatToStr(PMatPrintIT.LowerCI[0], 4) +
				AnsiString(PMatPrintIT.LowerCIWarning[0].c_str()) + " - " + MyFloatToStr(PMatPrintIT.UpperCI[0], 4) +
				AnsiString(PMatPrintIT.UpperCIWarning[0].c_str()) + ")";
			end_of_document->set_Text(StringToOleStr(param));
			end_of_document->InsertParagraphAfter();
			GetEndOfDocument(wordapplication, word_document, end_of_document);
		}

		if (PMatIT.Fit[1])
		{
			param = "mw:\t" + MyFloatToStr(PMatPrintIT.BestValue[1], 4) + "\t(" + MyFloatToStr(PMatPrintIT.LowerCI[1], 4) +
				AnsiString(PMatPrintIT.LowerCIWarning[1].c_str()) + " - " + MyFloatToStr(PMatPrintIT.UpperCI[1], 4) +
				AnsiString(PMatPrintIT.UpperCIWarning[1].c_str()) + ")";
			end_of_document->set_Text(StringToOleStr(param));
			end_of_document->InsertParagraphAfter();
			GetEndOfDocument(wordapplication, word_document, end_of_document);
		}

		if (PMatIT.Fit[2])
		{
			param = "hb:\t" + MyFloatToStr(PMatPrintIT.BestValue[2], 4) + "\t(" + MyFloatToStr(PMatPrintIT.LowerCI[2], 4) +
				AnsiString(PMatPrintIT.LowerCIWarning[2].c_str()) + " - " + MyFloatToStr(PMatPrintIT.UpperCI[2], 4) +
				AnsiString(PMatPrintIT.UpperCIWarning[2].c_str()) + ")";
			end_of_document->set_Text(StringToOleStr(param));
			end_of_document->InsertParagraphAfter();
			GetEndOfDocument(wordapplication, word_document, end_of_document);
		}

		if (PMatIT.Fit[4])
		{
			param = "Fs:\t" + MyFloatToStr(PMatPrintIT.BestValue[4], 4) + "\t(" + MyFloatToStr(PMatPrintIT.LowerCI[4], 4) +
				AnsiString(PMatPrintIT.LowerCIWarning[4].c_str()) + " - " + MyFloatToStr(PMatPrintIT.UpperCI[4], 4) +
				AnsiString(PMatPrintIT.UpperCIWarning[4].c_str()) + ")";
			end_of_document->set_Text(StringToOleStr(param));
			end_of_document->InsertParagraphAfter();
			GetEndOfDocument(wordapplication, word_document, end_of_document);
		}

	}

	range_of_inserted_text->End = end_of_document->End;
	end_of_document->InsertParagraphAfter();
	return range_of_inserted_text;

}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::ProjectNameEditExit(TObject *Sender)
{
	string test_name = AnsiString(ProjectNameEdit->Text).c_str();
	int i = 0;
	while (test_name[i])
	{
		if (test_name[i] == '\\' || test_name[i] == '/' || test_name[i] == ':' || test_name[i] == '*' || test_name[i] == '?' ||
			test_name[i] == '\"' || test_name[i] == '<' || test_name[i] == '>' || test_name[i] == '|')
		{
			test_name.erase(i, i + 1);
			i--;
		}
		i++;
	}

	ProjectTitle = test_name.c_str();
}
// ---------------------------------------------------------------------------

AnsiString TMainForm::MyFloatToStr(double float_num, int significant_digits)
{
	AnsiString float_astr = FloatToStr(float_num);
	string float_str = float_astr.c_str();
	vector<string> svP, svE;
	double return_dbl;
	int count = 0;
	bool start_count = false;
	bool point_found = false;
	string return_str = "";
	for (int i = 0; i <= float_str.length(); i++)
	{
		if (float_str.substr(i, 1) != "0" && float_str.substr(i, 1) != ".")
			start_count = true;
		if (start_count)
		{
			if (float_str.substr(i, 1) != ".")
				count++;
		}
		if (float_str.substr(i, 1) == ".")
			point_found = true;

		if (count >= significant_digits && point_found)
		{
			if (float_str.substr(i, 1) == ".")
				return_str = float_str.substr(0, i);
			else
				return_str = float_str.substr(0, i+1);

			// round if neccessary
			svP = SplitString(return_str, ".");
			svE = SplitString(float_str, "E");
			return_dbl = StrToFloat(svE[0].c_str());
			if (svP.size() == 1)
				return_dbl = RoundTo(return_dbl, 0);
			if (svP.size() == 2)
				return_dbl = RoundTo(return_dbl, -1*svP[1].size());
			return_str = AnsiString(return_dbl).c_str();

			// append the E-x term
			if (svE.size() == 2)
			{
				svE[1].insert(svE[1].begin(), 'E');
				return_str.insert(return_str.end(), svE[1].begin(), svE[1].end());
			}

			return AnsiString(return_str.c_str());
		}
	}
	return float_astr;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::ValidDataOpenSpeedClick(TObject *Sender)
{
	if (InputFileOpenDialog->Execute())
	{
		OpenConcSurvInputDataFile(AnsiString(InputFileOpenDialog->FileName).c_str(), ValidationDataDescriptionRichEdit,
				ValidationSurvStringGrid, ValidationConcStringGrid, ConcUnitEdit, ValidControlCombo);
		ValidationDataFileName = AnsiString(InputFileOpenDialog->FileName);
        ClearValidationResults();
	}

    return;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::ValidDataSaveSpeedClick(TObject *Sender)
{
	if (InputFileSaveDialog->Execute())
		SaveConcSurvInputDataFile(AnsiString(InputFileSaveDialog->FileName).c_str(), ValidationDataDescriptionRichEdit,
		ValidationSurvStringGrid, ValidationConcStringGrid, ConcUnitEdit);
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::ValidationBGMFitCheckBoxClick(TObject *Sender)
{
	FitHBToValidationControl = ValidationBGMFitCheckBox->Checked;

	ClearValidationResults();

	return;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::acCreateReportExecute(TObject *Sender)
{
	if (!CalibrationPerformed)
	{
		ShowMessage("Please perform at least a calibration before creating reports! Previous calibration results may have been rejected due to manipulations in the input data tab.");
		return;
	}

	if (CountProcesses("WINWORD.EXE") > 0)
	{
		AnsiString as = (AnsiString)"Warning: All instances of MS Word have to be closed before creating reports." +
						"\n\nPress 'OK' to proceed (CHANGES IN ACTIVE DOCUMENTS WILL BE LOST)." +
						"\nPress 'Cancel' to close all instances of MS Word manually.";
		if (MessageDlg(as, mtWarning, TMsgDlgButtons() << mbOK << mbCancel, 0) != mrOk)
			return;
		else
			MainForm->KillMSWordProcesses();
	}

	if (!SaveReportDialog->Execute())
		return;

	Screen->Cursor = crHourGlass;

	// prepare word:
	TWordApplication *word_application = NULL;
	TWordDocument *word_document = NULL;
	word_application = new TWordApplication(this);         	// create new word program instance
	word_document = new TWordDocument(word_application); 	// create new word document
	if (!ConnectAndActivateWordApplicationAndDocument(word_application, word_document))
	{
		Screen->Cursor = crDefault;
		return;
	}

	// setup word document (e.g. font type)
	SetupWordDocumentProperties(word_application, word_document);

	// write report
	string word_file = c_str(SaveReportDialog->FileName);

	WriteReport(word_application, word_document, word_file);

	// save report
	if (!FileExists(word_file.c_str()))
		word_document->SaveAs(&TVariant(WideString(word_file.c_str())),OleVariant(16)); // 16 - wdOriginalDocumentFormat
	word_document->SaveAs(&TVariant(WideString(word_file.c_str())));

	word_application->set_Visible(true);
	word_application->Disconnect();

	delete word_document;
	delete word_application;

	if (!ConvertToPDF(word_file))
		ShowMessage("Creation of PDF report failed!\n\nPlease ensure that:\n- the 'docto.exe' file for PDF conversion is in the same folder as the 'openGUTS.exe'\n- after installation of a new version of 'openGUTS' the previous version was completely removed from the system\n- the program 'docto.exe' is not blocked by an anti-virus program\n\nIn case the problem persists, the pdf-report can alternatively be generated from 'Microsoft word'.");

	Screen->Cursor = crDefault;

	return;
}
// ---------------------------------------------------------------------------

void TMainForm::SetupWordDocumentProperties(TWordApplication* word_application,
			TWordDocument *word_document)
{
	// setup start settings
	word_document->Paragraphs->set_LineSpacing(12*1.1);   	// line spacing in points; one line has 12 points

	return;
}
// ---------------------------------------------------------------------------

bool TMainForm::ConnectAndActivateWordApplicationAndDocument(TWordApplication *word_application,
			TWordDocument *word_document)
{
	try
	{
		word_application->Connect();                            // connect to new word document
	}
	catch (...)
	{
		Screen->Cursor = crArrow;
		ShowMessage("Could not access MS Word");
		if (word_document != NULL) delete word_document;
		if (word_application != NULL) delete word_application;
		return false;
	}
	try
	{
		word_document->Activate();
	}
	catch (...)
	{
		Screen->Cursor=crArrow;
		ShowMessage("MS Word is waiting for user input. Please switch to MS Word or try again later.");
		if (word_document != NULL) delete word_document;
		if (word_application != NULL) delete word_application;
		return false;
	}
	if (!word_application || !word_document)
	{
		Screen->Cursor=crArrow;
		ShowMessage("Could not access MS Word. Please switch to MS Word or try again later.");
		if (word_document != NULL) delete word_document;
		if (word_application != NULL) delete word_application;
		return false;
	}

	return true;
}
// ---------------------------------------------------------------------------

bool TMainForm::WriteReport(TWordApplication *word_application,
			TWordDocument *word_document, string filename)
{
	AnsiString error;

	ProjectTitle = BaseFileName(filename);

	// basic information
	CreateBasicInformationReport(word_application, word_document, error);

	// calibration
	CreateCalibrationReport(word_application, word_document, error);

	// validation
	CreateValidationReport(word_application, word_document, error);

	// predictions
	if (LPxBatch_out.size() > 0)
	{
        // Batch predictions are available and can be reported
		CreateBatchPredictionReport(word_application, word_document, error);
	}
	CreatePredictionReport(word_application, word_document, error);

	return true;
}
// ---------------------------------------------------------------------------

void TMainForm::CreateAndOpenWordFile(TWordApplication* wordapplication,
		TWordDocument* word_document, string filename, AnsiString &error)
{
	// create empty word file
	AnsiString as = "type NUL > " + AnsiString(filename.c_str());
	system(as.c_str());

	// open the word file
	OpenWordApplicationAndSaveDocument(wordapplication, word_document, filename, error);

	return;
}
// ---------------------------------------------------------------------------

bool TMainForm::ConvertToPDF(string word_file)
{
	AnsiString command, full_exe_path;

	// get path and change file extension (doc or docx to pdf)
	vector<string> sv;
	sv = SplitString(word_file, ".");
	if (sv.size() < 2)
		return false;
	int ext_length = sv[sv.size()-1].length();
	string pdf_file = word_file.substr(0, word_file.length()-ext_length) + "pdf";

	wchar_t executable_path[MAX_PATH];

	// get full path of executable
	GetModuleFileName(NULL, executable_path, MAX_PATH);

	full_exe_path = AnsiString(executable_path);
	string exe_path_without_filename = PathWithoutFile(full_exe_path.c_str());

	

	command = "\"" + AnsiString(exe_path_without_filename.c_str()) + "docto.exe\" -f \"" +
				AnsiString(word_file.c_str()) + "\" -o \"" +
				AnsiString(pdf_file.c_str()) + "\" -t wdFormatPDF";

	if (WinExec(command.c_str(), SW_HIDE) > 31)     // If the function succeeds, the return value is greater than 31
		return true;

	return false;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::acBatchExecute(TObject *Sender)
{
    if (!MainForm->CalibrationPerformed)
	{
		MessageDlg("A prediction can only be performed after a successful calibration of the (unchanged) input data sets! Please load a project file containing calibration results or conduct a calibration with suitable data.", mtInformation, TMsgDlgButtons() << mbOK, 0);
		return;
	}
	BatchPredictionDlg->LabelSuccess->Visible = false;
	BatchPredictionDlg->Caption = "Batch prediction - " + MainForm->Caption;
	BatchPredictionDlg->ShowModal();
}
// ---------------------------------------------------------------------------

bool TMainForm::GetPredictionsExposureDataFromFile(vector<double> &time_vec,
	vector<double> &conc_vec, string filename)
{
	// open predictions data file and get time and concentration vectors
	/*
		 Data format as received from Tjalling Jager:
		 0.000	1.379E-03
		 0.042	8.058E-03
		 0.083	1.496E-02
		 0.125	2.083E-02
		 0.167	2.585E-02
		 ...  	...
		 */

	ifstream ifile(filename.c_str());
	string s;
	vector<string>sv;

	time_vec.resize(0);
	conc_vec.resize(0);

	while (!ifile.eof())
	{
		GetNoCommentLine(ifile, s);
		sv = SplitString(s, "\t");
		if (sv.size() == 2)
		{
			if (!StringIsDouble(sv[0]) || !StringIsDouble(sv[1]))
			{
				ShowMessage("Error in file '" + AnsiString(filename.c_str()) +
						"'. Required data format: Two tab-separated columns containing time and concentration (no headers allowed).");
				return false;
			}
			time_vec.push_back(StrToFloat(sv[0].c_str()));
			conc_vec.push_back(StrToFloat(sv[1].c_str()));
		}
	}
	if (time_vec.size() < 1 || fabs(time_vec[0]) > EPS)
	{
		ShowMessage("Error in file '" + AnsiString(filename.c_str()) +
				"'. Required data format: Two tab-separated columns containing time (non-zero first element) and concentration (no headers allowed).");
		return false;
	}

	return true;
}
// ---------------------------------------------------------------------------

bool TMainForm::RunBatch(const vector<TParameterSpace>& SDParaSample, const vector<TParameterSpace>& ITParaSample, AnsiString &err)
{
	if (BatchFiles.size() < 1)
	{
		return false;
	}

	Screen->Cursor = crHourGlass;

	ChDir(BatchOutputFolder.c_str());
	vector<double> time_vec, conc_vec;

	double progress_bar_position = 0;
	double progress_bar_step = 100.0 / (3.0 * BatchFiles.size());

	BatchPredictionDlg->BatchProgressBar->Position = 0;
	BatchPredictionDlg->BatchProgressLabel->Caption = "";
	Application->ProcessMessages();
	BatchOutputString = "LPx values for different scenarios";

		BatchOutputString += (AnsiString)"\r\n  Special case:\t" +
						 (DoSDBatchPredictions?"stochastic death (SD)":"") +
						 (DoSDBatchPredictions && DoITBatchPredictions ?" and ":"") +
						 (DoITBatchPredictions?"individual tolerance (IT)":"") +
						 "\r\n";
	BatchPredictionDlg->BatchMemo->Lines->Add(BatchOutputString);

	if (BatchPredictionDlg -> LPxCalcCIBatchCheckBox -> Checked)
	{
		BatchOutputString = (AnsiString)
		BatchOutputString = (AnsiString)"File" +
			(DoSDBatchPredictions?"\tLP10 (SD)\t0.025 CI\t0.975 CI\tLP50 (SD)\t0.025 CI\t0.975 CI":"") +
						 (DoITBatchPredictions?"\tLP10 (IT)\t0.025 CI\t0.975 CI\tLP50 (IT)\t0.025 CI\t0.975 CI":"");

	}
	else
	{
		BatchOutputString = (AnsiString)"File" + (DoSDBatchPredictions?"\tLP10 (SD)\tLP50 (SD)":"") +
						 (DoITBatchPredictions?"\tLP10 (IT)\tLP50 (IT)":"");
	}

	BatchPredictionDlg->BatchMemo->Lines->Add(BatchOutputString);

	// Every batch run deletes previous results
	LPxBatch_out.assign(BatchFiles.size() + 1, "");
	LPxBatch_out[0] = (BatchOutputString);

	for (int f = 0; f < BatchFiles.size(); f++)
	{
		Application->ProcessMessages();
		if (StopBatchPredictions)
		{
			break;
		}

		BatchPredictionDlg->BatchProgressLabel->Caption = ("..." + BatchFiles[f].substr(Max(0, BatchFiles[f].length()-20), BatchFiles[f].length())).c_str();
		Application->ProcessMessages();

		if (!GetPredictionsExposureDataFromFile(time_vec, conc_vec, BatchFiles[f]))
		{
			BatchPredictionDlg->BatchProgressBar->Position = 0;
			Screen->Cursor = crDefault;
			return false;
		}
		PreparePredictionsFileData(time_vec, conc_vec);

		Application->ProcessMessages();
		if (StopBatchPredictions)
		{
			break;
		}
		UpdateBatchProgressBar(BatchPredictionDlg->BatchProgressBar, progress_bar_position, progress_bar_step);

		BatchOutputString = AnsiString(BaseFileName(BatchFiles[f]).c_str());
		if (!PredictFromBatchFile(BatchFiles[f], SDParaSample, ITParaSample, err))
		{
			ShowMessage(err);
			BatchPredictionDlg->BatchProgressBar->Position = 0;
			Screen->Cursor = crDefault;
			return false;
		}
		BatchPredictionDlg->BatchMemo->Lines->Add(BatchOutputString);
		LPxBatch_out[f+1] = BatchOutputString;

		Application->ProcessMessages();
		if (StopBatchPredictions)
		{
			break;
		}
		UpdateBatchProgressBar(BatchPredictionDlg->BatchProgressBar, progress_bar_position, progress_bar_step);
	}
	double finished = 100.0;
	UpdateBatchProgressBar(BatchPredictionDlg->BatchProgressBar, finished, 0);
	if (StopBatchPredictions)
		BatchPredictionDlg->BatchProgressLabel->Caption = "Batch prediction stopped!";
	else
		BatchPredictionDlg->BatchProgressLabel->Caption = "Batch prediction finished!";

	finished = 0.0;
	UpdateBatchProgressBar(BatchPredictionDlg->BatchProgressBar, finished, 0);

	//BatchPredictionDlg->BatchMemo->Lines->Add(BatchOutputString);
	StopBatchPredictions = false;

	Screen->Cursor = crDefault;

	return true;
}
// ---------------------------------------------------------------------------

void TMainForm::UpdateBatchProgressBar(TProgressBar *progress_bar, double &position, double step)
{
	position += step;
	progress_bar->Position = progress_bar->Max;
	progress_bar->Position = RoundTo(position, 0);
	Application->ProcessMessages();

	return;
}
// ---------------------------------------------------------------------------

string TMainForm::BaseFileName(string filename)
{
	vector<string>sv;

	// get rid of path:
	sv = SplitString(filename, "\\");
	filename = sv[sv.size() - 1];
	// get rid of extension:
	sv = SplitString(filename, ".");
	string s = "";
	if (sv.size() > 2)
		for (int i = 0; i < sv.size() - 1; i++)
			s += sv[i];
	filename = sv[0];

	return filename;
}
// ---------------------------------------------------------------------------

AnsiString TMainForm::BaseFileName(AnsiString filename)
{
	string in = filename.c_str();
	string out = BaseFileName(in);

	return AnsiString(out.c_str());
}
// ---------------------------------------------------------------------------

string TMainForm::PathWithoutFile(string filename)
{
	vector<string>sv;
	string pathname = filename;

	// get rid of file name:
	sv = SplitString(filename, "\\");
	int file_name_length = sv[sv.size() - 1].length();
	pathname = pathname.substr(0, pathname.length()-file_name_length);

	return pathname;
}
// ---------------------------------------------------------------------------

bool TMainForm::PredictFromBatchFile(string filename, const vector<TParameterSpace>& SDParaSample, const vector<TParameterSpace>& ITParaSample, AnsiString &err)
{
	vector<vector<double> > LPx_1, LP_low_1, LP_high_1, LPx_2, LP_low_2, LP_high_2;
	int flag; // LPx calculation without calculation of CI
	if (BatchPredictionDlg -> LPxCalcCIBatchCheckBox -> Checked)
		flag = 1; 	// regular LPx calulation with calculation of CI
	else
		flag = 2;   // no CI (very fast)

	double hb_fix = -1;
	if (DoSDBatchPredictions)
	{
		DoAndPaintPredictions(LPx_1, LP_low_1, LP_high_1, PredictionsData_All, SDParaSample, flag, gutsSD, BatchPredictionsMemoStringsSD, hb_fix, err);
		if (err != "")
			return false;
		if (StopBatchPredictions)
		{
			return true;
		}
		// SDPredictionsPerformed = true;
		if (flag == 2) {
		   BatchOutputString += (AnsiString)"\t" + MyFloatToStr(LPx_1[0][0],4) + "\t" + MyFloatToStr(LPx_1[0][1],4);
		} else {
		   BatchOutputString += (AnsiString)"\t"
		   + MyFloatToStr(LPx_1[0][0],4) + "\t" + MyFloatToStr(LP_low_1[0][0],4) + "\t" + MyFloatToStr(LP_high_1[0][0],4) + "\t"
		   + MyFloatToStr(LPx_1[0][1],4) + "\t" + MyFloatToStr(LP_low_1[0][1],4) + "\t" + MyFloatToStr(LP_high_1[0][1],4);
		}
	}
	if (DoITBatchPredictions)
	{
		DoAndPaintPredictions(LPx_2, LP_low_2, LP_high_2, PredictionsData_All, ITParaSample, flag, gutsIT, BatchPredictionsMemoStringsIT, hb_fix, err);
		if (err != "")
			return false;
		if (StopBatchPredictions)
		{
			return true;
		}
		// ITPredictionsPerformed = true;
        if (flag == 2) {
		   BatchOutputString += (AnsiString)"\t" + MyFloatToStr(LPx_2[0][0],4) + "\t" + MyFloatToStr(LPx_2[0][1],4);
		} else {
		   BatchOutputString += (AnsiString)"\t"
		   + MyFloatToStr(LPx_2[0][0],4) + "\t" + MyFloatToStr(LP_low_2[0][0],4) + "\t" + MyFloatToStr(LP_high_2[0][0],4) + "\t"
		   + MyFloatToStr(LPx_2[0][1],4) + "\t" + MyFloatToStr(LP_low_2[0][1],4) + "\t" + MyFloatToStr(LP_high_2[0][1],4);
		}
	}

	return true;
}
// ---------------------------------------------------------------------------

void TMainForm::DoAndPaintPredictions(vector<vector<double> > &lpx, vector<vector<double> > &lp_low,
	vector<vector<double> > &lp_high, TPreparedDataContainer data_all, const vector<TParameterSpace>& paraSample, int flag, int sdit, vector<AnsiString> &memo_strings,
	double hb_fix, AnsiString &err)
{
	// CALCULATE LPx
	TScrollBox *dummy_scrollbox = new TScrollBox(this);
	TMemo *dummy_memo = new TMemo(this);
	Calc_LPx(lpx, lp_low, lp_high, data_all.Concentration[0], flag, sdit, BATCH_PREDICTION, dummy_memo, memo_strings, err, paraSample);
	if (err != "")
		return;

	// ADD LPx DATA to PredictionsData_All
	data_all.Concentration[0].resize(lpx[0].size() + 1);
	data_all.GroupNames[0].resize(lpx[0].size() + 1);
	data_all.GroupNames[0][0] = "MF = 1";
	for (int g = 1; g < data_all.Concentration[0].size(); g++)
	{
		data_all.Concentration[0][g] = data_all.Concentration[0][0];
		data_all.GroupNames[0][g] = "MF = " + MyFloatToStr(lpx[0][g - 1], 4);
		for (int t = 0; t < data_all.Concentration[0][g].Times.size(); t++)
		{
			Application->ProcessMessages();
			data_all.Concentration[0][g].Conc[t] *= lpx[0][g - 1];
			data_all.Concentration[0][g].Slope[t] *= lpx[0][g - 1];
		}
	}

	// PAINTING
	// GUTS graphs (exposure, damage, survival)
	TImage *batch_img;
	PaintGUTSGraphs(data_all, 1, hb_fix, dummy_scrollbox, batch_img, BATCH_PREDICTION, sdit, err);
	if (err != "")
		return;

	// LPx multiplication factors
	if (sdit == gutsSD)
		PlotLPx(BatchPredictionMultiFacImageSD, sdit, false);
	else
		PlotLPx(BatchPredictionMultiFacImageIT, sdit, false);

	delete dummy_memo;
	delete dummy_scrollbox;

	return;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::FileSaveAsItemClick(TObject *Sender)
{
	if (!ProjectFileSaveDialog->Execute())
		return;
	ProjectFileName = ProjectFileSaveDialog->FileName;
	if (SaveProjectToFile(AnsiString(ProjectFileName).c_str()))
		Caption = SOFTWARE_NAME + " " + SOFTWARE_VERSION + "  - " + ProjectFileName;

    return;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibSurvStringGrid1Exit(TObject *Sender)
{
	UpdateControlGroupCombo(CalibControlCombo1, CalibSurvStringGrid1);
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibSurvStringGrid2Exit(TObject *Sender)
{
	UpdateControlGroupCombo(CalibControlCombo2, CalibSurvStringGrid2);
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibSurvStringGrid3Exit(TObject *Sender)
{
	UpdateControlGroupCombo(CalibControlCombo3, CalibSurvStringGrid3);
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibSurvStringGrid4Exit(TObject *Sender)
{
	UpdateControlGroupCombo(CalibControlCombo4, CalibSurvStringGrid4);
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibSurvStringGrid5Exit(TObject *Sender)
{
	UpdateControlGroupCombo(CalibControlCombo5, CalibSurvStringGrid5);
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ValidationSurvStringGridExit(TObject *Sender)
{
	UpdateControlGroupCombo(ValidControlCombo, ValidationSurvStringGrid);
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::AbortCalibrationButtonClick(TObject *Sender)
{
	AbortCalibration = true;

	return;
}
//---------------------------------------------------------------------------

void TMainForm::EraseEmptyStrings(vector<string> &sv)
{
	for (unsigned int i=0; i<sv.size(); i++)
		if (sv[i] == "")
		{
			sv.erase(sv.begin() + i);
			i--;
		}

	return;
}
//---------------------------------------------------------------------------

void TMainForm::PreparePredictionsFileData(vector<double> &time_vec, vector<double> &conc_vec)
{
	// setup data structure for further processing
	TGridData grid_data;
	grid_data.GroupNames.resize(1);
	grid_data.GroupNames[0] = "";
	grid_data.Times = time_vec;
	grid_data.Values.resize(1);
	grid_data.Values[0] = conc_vec;

	// build data structure
	PredictionsData.resize(1);
	PredictionsData[0].Concentration = grid_data;
	// add dummy survival data as proposed by Tjalling Jager
	PredictionsData[0].Survival.Times.resize(2);
	PredictionsData[0].Survival.Times[0] = 0;
	PredictionsData[0].Survival.Times[1] = time_vec[time_vec.size()-1];

	// prepare data
	Prepare_Data(PredictionsData_All, PredictionsData_HB, PredictionsData,
			PredictionsData.size(), false);

	return;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CopySDParamsButtonClick(TObject *Sender)
{
	AnsiString as = "";
	for (int p=0; p<gutsPARAMETER_NO; p++)
	{
		as += AnsiString(gutsPARAMETERS[p].c_str()) + "\t" + AnsiString(GUTSSDParamValueEdits[p]->Text) + "\r\n";
	}
	Clipboard()->SetTextBuf(WideString(as).c_bstr());

	return;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CopyITParamsButtonClick(TObject *Sender)
{
	AnsiString as = "";
	for (int p=0; p<gutsPARAMETER_NO; p++)
	{
		as += AnsiString(gutsPARAMETERS[p].c_str()) + "\t" + AnsiString(GUTSITParamValueEdits[p]->Text) + "\r\n";
	}
	Clipboard()->SetTextBuf(WideString(as).c_bstr());

	return;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibConcStringGrid1Exit(TObject *Sender)
{
	UpdateControlGroupCombo(CalibControlCombo1, CalibConcStringGrid1);
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibConcStringGrid2Exit(TObject *Sender)
{
    UpdateControlGroupCombo(CalibControlCombo2, CalibConcStringGrid2);
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibConcStringGrid3Exit(TObject *Sender)
{
    UpdateControlGroupCombo(CalibControlCombo3, CalibConcStringGrid3);
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibConcStringGrid4Exit(TObject *Sender)
{
    UpdateControlGroupCombo(CalibControlCombo4, CalibConcStringGrid4);
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibConcStringGrid5Exit(TObject *Sender)
{
    UpdateControlGroupCombo(CalibControlCombo5, CalibConcStringGrid5);
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ValidationConcStringGridExit(TObject *Sender)
{
	UpdateControlGroupCombo(ValidControlCombo, ValidationConcStringGrid);
}
//---------------------------------------------------------------------------


void __fastcall TMainForm::CalibConcStringGrid1KeyPress(TObject *Sender, System::WideChar &Key)

{
	CheckIfTopLeftCellEdited((TStringGrid*)Sender, Key);
	CalibrationDataFileNames[0] = "Data edited manually.";
}
//---------------------------------------------------------------------------

bool TMainForm::CheckIfTopLeftCellEdited(TStringGrid *grid, System::WideChar &Key)
{
	UnicodeString old_text = grid->Cells[0][0];
	if (grid->Col == 0 && grid->Row == 0)
	{
		Key = 27;
		ShowMessage("The header '" + old_text + "' should not be changed!");
		grid->Row = 1;
		return true;
	}

	return false;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibSurvStringGrid1KeyPress(TObject *Sender, System::WideChar &Key)

{
	CheckIfTopLeftCellEdited((TStringGrid*)Sender, Key);
	CalibrationDataFileNames[0] = "Data edited manually.";
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibConcStringGrid2KeyPress(TObject *Sender, System::WideChar &Key)

{
	CheckIfTopLeftCellEdited((TStringGrid*)Sender, Key);
	CalibrationDataFileNames[1] = "Data edited manually.";
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibSurvStringGrid2KeyPress(TObject *Sender, System::WideChar &Key)

{
	CheckIfTopLeftCellEdited((TStringGrid*)Sender, Key);
	CalibrationDataFileNames[1] = "Data edited manually.";
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibConcStringGrid3KeyPress(TObject *Sender, System::WideChar &Key)

{
	CheckIfTopLeftCellEdited((TStringGrid*)Sender, Key);
	CalibrationDataFileNames[2] = "Data edited manually.";
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibSurvStringGrid3KeyPress(TObject *Sender, System::WideChar &Key)

{
	CheckIfTopLeftCellEdited((TStringGrid*)Sender, Key);
	CalibrationDataFileNames[2] = "Data edited manually.";
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibConcStringGrid4KeyPress(TObject *Sender, System::WideChar &Key)

{
	CheckIfTopLeftCellEdited((TStringGrid*)Sender, Key);
	CalibrationDataFileNames[3] = "Data edited manually.";
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibSurvStringGrid4KeyPress(TObject *Sender, System::WideChar &Key)

{
	CheckIfTopLeftCellEdited((TStringGrid*)Sender, Key);
    CalibrationDataFileNames[3] = "Data edited manually.";
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibConcStringGrid5KeyPress(TObject *Sender, System::WideChar &Key)

{
	CheckIfTopLeftCellEdited((TStringGrid*)Sender, Key);
	CalibrationDataFileNames[4] = "Data edited manually.";
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibSurvStringGrid5KeyPress(TObject *Sender, System::WideChar &Key)

{
	CheckIfTopLeftCellEdited((TStringGrid*)Sender, Key);
	CalibrationDataFileNames[4] = "Data edited manually.";
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ValidationConcStringGridKeyPress(TObject *Sender, System::WideChar &Key)

{
	CheckIfTopLeftCellEdited((TStringGrid*)Sender, Key);
    ValidationDataFileName = "Data edited manually.";
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ValidationSurvStringGridKeyPress(TObject *Sender, System::WideChar &Key)

{
	CheckIfTopLeftCellEdited((TStringGrid*)Sender, Key);
	ValidationDataFileName = "Data edited manually.";
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ConcUnitEditChange(TObject *Sender)
{
	ToDoAfterInputDataManipulation();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::InputDataDescriptionRichEdit1Change(TObject *Sender)
{
	//ToDoAfterInputDataManipulation();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibConcStringGrid1Click(TObject *Sender)
{
	ToDoAfterInputDataManipulation();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibSurvStringGrid1Click(TObject *Sender)
{
	ToDoAfterInputDataManipulation();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::InputDataDescriptionRichEdit2Change(TObject *Sender)
{
	//ToDoAfterInputDataManipulation();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibConcStringGrid2Click(TObject *Sender)
{
	ToDoAfterInputDataManipulation();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibSurvStringGrid2Click(TObject *Sender)
{
	ToDoAfterInputDataManipulation();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::InputDataDescriptionRichEdit3Change(TObject *Sender)
{
	//ToDoAfterInputDataManipulation();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibConcStringGrid3Click(TObject *Sender)
{
	ToDoAfterInputDataManipulation();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibSurvStringGrid3Click(TObject *Sender)
{
	ToDoAfterInputDataManipulation();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::InputDataDescriptionRichEdit4Change(TObject *Sender)
{
	//ToDoAfterInputDataManipulation();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibConcStringGrid4Click(TObject *Sender)
{
	ToDoAfterInputDataManipulation();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibSurvStringGrid4Click(TObject *Sender)
{
	ToDoAfterInputDataManipulation();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::InputDataDescriptionRichEdit5Change(TObject *Sender)
{
	//ToDoAfterInputDataManipulation();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibConcStringGrid5Click(TObject *Sender)
{
	ToDoAfterInputDataManipulation();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibSurvStringGrid5Click(TObject *Sender)
{
	ToDoAfterInputDataManipulation();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibControlCombo1Change(TObject *Sender)
{
	ToDoAfterInputDataManipulation();
	Reset(false);
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibControlCombo2Change(TObject *Sender)
{
	ToDoAfterInputDataManipulation();
	Reset(false);
}
//---------------------------------------------------------------------------


void __fastcall TMainForm::CalibControlCombo3Change(TObject *Sender)
{
	ToDoAfterInputDataManipulation();
	Reset(false);
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibControlCombo4Change(TObject *Sender)
{
	ToDoAfterInputDataManipulation();
	Reset(false);
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CalibControlCombo5Change(TObject *Sender)
{
	ToDoAfterInputDataManipulation();
	Reset(false);
}
//---------------------------------------------------------------------------

void TMainForm::ToDoAfterInputDataManipulation()
{
  /*	CalibrationPerformed = false;
	CalibrationStatusLabel->Font->Color = clBlue;
	CalibrationStatusLabel->Caption = "No calibration performed.";

	ParameterRangesManipulated = false;*/
}
//---------------------------------------------------------------------------


void __fastcall TMainForm::ITParamHBFitCheckClick(TObject *Sender)
{
	if (!ITParamHBFitCheck->Checked)
	{
		ITParamHBMinEdit->Text = ITParamHBValueEdit->Text;
		ITParamHBMaxEdit->Text = ITParamHBValueEdit->Text;
		ITParamHBMinEdit->Enabled = false;
		ITParamHBMaxEdit->Enabled = false;
	}
	else
	{
		ITParamHBMinEdit->Enabled = true;
		ITParamHBMaxEdit->Enabled = true;
	}
	if (!ParameterRangesManipulated)
		UpdateParameterRangesBasedOnInputData();
	if (PrefitHBToControl)
		ITParamHBFitCheck->Checked = false;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::SDParamHBFitCheckClick(TObject *Sender)
{
	if (!SDParamHBFitCheck->Checked)
	{
		SDParamHBMinEdit->Text = SDParamHBValueEdit->Text;
		SDParamHBMaxEdit->Text = SDParamHBValueEdit->Text;
		SDParamHBMinEdit->Enabled = false;
		SDParamHBMaxEdit->Enabled = false;
	}
	else
	{
		SDParamHBMinEdit->Enabled = true;
		SDParamHBMaxEdit->Enabled = true;
	}
	if (!ParameterRangesManipulated)
		UpdateParameterRangesBasedOnInputData();
	if (PrefitHBToControl)
		SDParamHBFitCheck->Checked = false;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::SDParamKDFitCheckClick(TObject *Sender)
{
	if (!SDParamKDFitCheck->Checked)
	{
		SDParamKDMinEdit->Text = SDParamKDValueEdit->Text;
		SDParamKDMaxEdit->Text = SDParamKDValueEdit->Text;
		SDParamKDMinEdit->Enabled = false;
		SDParamKDMaxEdit->Enabled = false;
	}
	else
	{
		SDParamKDMinEdit->Enabled = true;
		SDParamKDMaxEdit->Enabled = true;
	}
	if (!ParameterRangesManipulated)
		UpdateParameterRangesBasedOnInputData();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::SDParamMWFitCheckClick(TObject *Sender)
{
	if (!SDParamMWFitCheck->Checked)
	{
		SDParamMWMinEdit->Text = SDParamMWValueEdit->Text;
		SDParamMWMaxEdit->Text = SDParamMWValueEdit->Text;
		SDParamMWMinEdit->Enabled = false;
		SDParamMWMaxEdit->Enabled = false;
	}
	else
	{
		SDParamMWMinEdit->Enabled = true;
		SDParamMWMaxEdit->Enabled = true;
	}
	if (!ParameterRangesManipulated)
		UpdateParameterRangesBasedOnInputData();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::SDParamBWFitCheckClick(TObject *Sender)
{
	if (!SDParamBWFitCheck->Checked)
	{
		SDParamBWMinEdit->Text = SDParamBWValueEdit->Text;
		SDParamBWMaxEdit->Text = SDParamBWValueEdit->Text;
		SDParamBWMinEdit->Enabled = false;
		SDParamBWMaxEdit->Enabled = false;
	}
	else
	{
		SDParamBWMinEdit->Enabled = true;
		SDParamBWMaxEdit->Enabled = true;
	}
	if (!ParameterRangesManipulated)
		UpdateParameterRangesBasedOnInputData();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ITParamKDFitCheckClick(TObject *Sender)
{
	if (!ITParamKDFitCheck->Checked)
	{
		ITParamKDMinEdit->Text = ITParamKDValueEdit->Text;
		ITParamKDMaxEdit->Text = ITParamKDValueEdit->Text;
		ITParamKDMinEdit->Enabled = false;
		ITParamKDMaxEdit->Enabled = false;
	}
	else
	{
		ITParamKDMinEdit->Enabled = true;
		ITParamKDMaxEdit->Enabled = true;
	}
	if (!ParameterRangesManipulated)
		UpdateParameterRangesBasedOnInputData();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ITParamMWFitCheckClick(TObject *Sender)
{
	if (!ITParamMWFitCheck->Checked)
	{
		ITParamMWMinEdit->Text = ITParamMWValueEdit->Text;
		ITParamMWMaxEdit->Text = ITParamMWValueEdit->Text;
		ITParamMWMinEdit->Enabled = false;
		ITParamMWMaxEdit->Enabled = false;
	}
	else
	{
		ITParamMWMinEdit->Enabled = true;
		ITParamMWMaxEdit->Enabled = true;
	}
	if (!ParameterRangesManipulated)
		UpdateParameterRangesBasedOnInputData();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ITParamFSFitCheckClick(TObject *Sender)
{
	if (!ITParamFSFitCheck->Checked)
	{
		ITParamFSMinEdit->Text = ITParamFSValueEdit->Text;
		ITParamFSMaxEdit->Text = ITParamFSValueEdit->Text;
		ITParamFSMinEdit->Enabled = false;
		ITParamFSMaxEdit->Enabled = false;
	}
	else
	{
		ITParamFSMinEdit->Enabled = true;
		ITParamFSMaxEdit->Enabled = true;
	}
	if (!ParameterRangesManipulated)
		UpdateParameterRangesBasedOnInputData();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::LCxDaysSDComboChange(TObject *Sender)
{
	if (!CalibrationPerformed)
		return;

	PlotLCx(CalibrationSDOutputLCxImage, gutsSD, 0.5+predT_LCX[LCxDaysSDCombo->ItemIndex]);

	return;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::LCxDaysITComboChange(TObject *Sender)
{
	if (!CalibrationPerformed)
		return;

	PlotLCx(CalibrationITOutputLCxImage, gutsIT, 0.5+predT_LCX[LCxDaysITCombo->ItemIndex]);

	return;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::SDParamKDMinEditChange(TObject *Sender)
{
	ParamEditChange();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::SDParamKDMaxEditChange(TObject *Sender)
{
	ParamEditChange();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::SDParamMWMinEditChange(TObject *Sender)
{
	ParamEditChange();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::SDParamMWMaxEditChange(TObject *Sender)
{
	ParamEditChange();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::SDParamHBMinEditChange(TObject *Sender)
{
	ParamEditChange();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::SDParamHBMaxEditChange(TObject *Sender)
{
	ParamEditChange();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::SDParamBWMinEditChange(TObject *Sender)
{
	ParamEditChange();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::SDParamBWMaxEditChange(TObject *Sender)
{
	ParamEditChange();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::SDParamFSMinEditChange(TObject *Sender)
{
	ParamEditChange();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::SDParamFSMaxEditChange(TObject *Sender)
{
	ParamEditChange();
}
//---------------------------------------------------------------------------

void TMainForm::ParamEditChange()
{
	if (!CurrentlyLoadingProjectFile)
		ParameterRangesManipulated = true;

	CalibrationPerformed = false;
	CalibrationStatusLabel->Font->Color = clBlue;
	CalibrationStatusLabel->Caption = "No calibration performed.";
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ITParamKDMinEditChange(TObject *Sender)
{
	ParamEditChange();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ITParamKDMaxEditChange(TObject *Sender)
{
	ParamEditChange();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ITParamMWMinEditChange(TObject *Sender)
{
	ParamEditChange();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ITParamMWMaxEditChange(TObject *Sender)
{
	ParamEditChange();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ITParamHBMinEditChange(TObject *Sender)
{
	ParamEditChange();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ITParamHBMaxEditChange(TObject *Sender)
{
	ParamEditChange();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ITParamBWMinEditChange(TObject *Sender)
{
	ParamEditChange();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ITParamBWMaxEditChange(TObject *Sender)
{
	ParamEditChange();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ITParamFSMinEditChange(TObject *Sender)
{
	ParamEditChange();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ITParamFSMaxEditChange(TObject *Sender)
{
	ParamEditChange();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::AbortPredictionsButtonClick(TObject *Sender)
{
	AbortPredictions = true;

	return;
}
//---------------------------------------------------------------------------

bool TMainForm::ResetCalibrationSettingInGUI()
// resets all calibration settings (parameter ranges, fit/ not fit, etc.)
{
	Screen->Cursor = crHourGlass;

	// check the data in the grids
	if (!CheckCalibrationDataSets())
		return false;

	// CHECK and GET the data from the grids
	CheckAndGetCalibrationData(CalibrationData);

	// prepare the calibration data sets
	Prepare_Data(CalibData_All, CalibData_HB, CalibrationData,
			NumberOfCalibrationDataSets, PrefitHBToControl);

	// derive default parameter ranges
	if (!Startgrid())
	{
		Screen->Cursor = crDefault;
		return false;
	}

	// reset fit/not fit and scale
	for (int p=0; p<gutsPARAMETER_NO; p++)
	{

		GUTSSDParamFitChecks[p]->Checked = calibSD_DEFAULT_BOOL_FIT[p];
		GUTSSDParamLogChecks[p]->Checked = calibSD_DEFAULT_BOOL_LOG[p];
		GUTSITParamFitChecks[p]->Checked = calibIT_DEFAULT_BOOL_FIT[p];
		GUTSITParamLogChecks[p]->Checked = calibIT_DEFAULT_BOOL_LOG[p];
	}

	// display the matrix in the GUI
	bool change_value = true;
	bool change_range = true;
	DisplayParameterMatrixInGUI(PMatSD, gutsSD, change_value, change_range);
	DisplayParameterMatrixInGUI(PMatIT, gutsIT, change_value, change_range);

	ParameterRangesManipulated = false;

	Screen->Cursor = crDefault;

	return true;
}
// ---------------------------------------------------------------------------

void TMainForm::KillMSWordProcesses()
{
	while (CountProcesses("WINWORD.EXE") > 0)
	{
		// make document visible (in case it is hidden):
		TWordApplication *wordapplication = new TWordApplication(Application);
		wordapplication->Connect();
		wordapplication->set_Visible(true);
		wordapplication->Disconnect();
		delete wordapplication;
		::Sleep(500);

		// kill MS Word process if it's still running:
		if (CountProcesses("WINWORD.EXE") > 0)
		{
			KillProcess("WINWORD.EXE");
			::Sleep(500);
		}
		break;
	}

}
// ---------------------------------------------------------------------------

bool TMainForm::TerminateMyProcess(DWORD dwProcessId, UINT uExitCode)
// helper function for function KillProcess()
{
	DWORD dwDesiredAccess = PROCESS_TERMINATE;
	bool bInheritHandle  = false;
	HANDLE hProcess = OpenProcess(dwDesiredAccess, bInheritHandle, dwProcessId);
	if (hProcess == NULL)
		return false;
	bool result = TerminateProcess(hProcess, uExitCode);
	CloseHandle(hProcess);
	return result;
}
// ---------------------------------------------------------------------------

int TMainForm::KillProcess(AnsiString executable_file)
// kills all processes with a given exe-name (file name)
{
	PROCESSENTRY32 *ProcEntry = new PROCESSENTRY32();
	ProcEntry->dwSize = sizeof(PROCESSENTRY32);
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	bool Result;
	Result=Process32First(hSnapshot, ProcEntry);
	int n=0;
	while (Result)
	{
		if (ExtractFileName(StrPas(ProcEntry->szExeFile))==executable_file)
		{ // process found
			n++;
			TerminateMyProcess(ProcEntry->th32ProcessID, 1);
		}
		Result = Process32Next(hSnapshot, ProcEntry);
	}
	CloseHandle(hSnapshot);
	delete(ProcEntry);
	return n;
}
// ---------------------------------------------------------------------------

void __fastcall TMainForm::SDParamKDValueEditChange(TObject *Sender)
{
	if (!SDParamKDFitCheck->Checked)
	{
		SDParamKDMinEdit->Text = SDParamKDValueEdit->Text;
		SDParamKDMaxEdit->Text = SDParamKDValueEdit->Text;
	}
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::SDParamMWValueEditChange(TObject *Sender)
{
	if (!SDParamMWFitCheck->Checked)
	{
		SDParamMWMinEdit->Text = SDParamMWValueEdit->Text;
		SDParamMWMaxEdit->Text = SDParamMWValueEdit->Text;
	}
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::SDParamHBValueEditChange(TObject *Sender)
{
	if (!SDParamHBFitCheck->Checked)
	{
		SDParamHBMinEdit->Text = SDParamHBValueEdit->Text;
		SDParamHBMaxEdit->Text = SDParamHBValueEdit->Text;
	}
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::SDParamBWValueEditChange(TObject *Sender)
{
	if (!SDParamBWFitCheck->Checked)
	{
		SDParamBWMinEdit->Text = SDParamBWValueEdit->Text;
		SDParamBWMaxEdit->Text = SDParamBWValueEdit->Text;
	}
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ITParamKDValueEditChange(TObject *Sender)
{
	if (!ITParamKDFitCheck->Checked)
	{
		ITParamKDMinEdit->Text = ITParamKDValueEdit->Text;
		ITParamKDMaxEdit->Text = ITParamKDValueEdit->Text;
	}
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ITParamMWValueEditChange(TObject *Sender)
{
	if (!ITParamMWFitCheck->Checked)
	{
		ITParamMWMinEdit->Text = ITParamMWValueEdit->Text;
		ITParamMWMaxEdit->Text = ITParamMWValueEdit->Text;
	}
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ITParamHBValueEditChange(TObject *Sender)
{
	if (!ITParamHBFitCheck->Checked)
	{
		ITParamHBMinEdit->Text = ITParamHBValueEdit->Text;
		ITParamHBMaxEdit->Text = ITParamHBValueEdit->Text;
	}
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ITParamFSValueEditChange(TObject *Sender)
{
	if (!ITParamFSFitCheck->Checked)
	{
		ITParamFSMinEdit->Text = ITParamFSValueEdit->Text;
		ITParamFSMaxEdit->Text = ITParamFSValueEdit->Text;
	}
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ValidControlComboChange(TObject *Sender)
{
	ClearValidationResults();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::WMGetMinMaxInfo(TWMGetMinMaxInfo &Message)
{
	Message.MinMaxInfo->ptMinTrackSize.x = 445; 	// Minimum window width
	Message.MinMaxInfo->ptMinTrackSize.y = 320; 	// Minimum window height, use 720 to see the concentration grid
	Message.Result = 0;
	TForm::Dispatch(&Message);

	return;
}
//---------------------------------------------------------------------------




