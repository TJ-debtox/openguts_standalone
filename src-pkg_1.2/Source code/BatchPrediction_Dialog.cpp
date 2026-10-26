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

#include "BatchPrediction_Dialog.h"
//---------------------------------------------------------------------
#pragma resource "*.dfm"
TBatchPredictionDlg *BatchPredictionDlg;
//---------------------------------------------------------------------
__fastcall TBatchPredictionDlg::TBatchPredictionDlg(TComponent* AOwner)
	: TForm(AOwner)
{
}
//---------------------------------------------------------------------
void __fastcall TBatchPredictionDlg::FormCreate(TObject *Sender)
{
	FileListBox->Clear();

	AddFileOpenDialog->DefaultExt = INPUT_DATA_FILE_EXT;
	AddFileOpenDialog->Filter = "Text file (*." + INPUT_DATA_FILE_EXT + ")|*." +
		INPUT_DATA_FILE_EXT + "|All Files (*.*)|*.*";
	AddFileOpenDialog->FilterIndex = 0;

	BatchProgressBar->Visible = false;
	BatchProgressLabel->Caption = "";

	LPxCalcCIBatchCheckBox->Checked = false;

    return;
}
//---------------------------------------------------------------------------

void __fastcall TBatchPredictionDlg::AddButtonClick(TObject *Sender)
{
	if (!AddFileOpenDialog->Execute())
		return;

	for (int f=0;f<AddFileOpenDialog->Files->Count; f++) {
		FileListBox->Items->Add(AddFileOpenDialog->Files->Strings[f]);
	}
}
//---------------------------------------------------------------------------

void TBatchPredictionDlg::DisableBatchDialog()
{
	UpperPanel->Enabled = false;
	OKBtn->Enabled = false;
	RunBtn->Enabled = false;
	StopBatchButton->Enabled = true;
	Application->ProcessMessages();

	return;
}
//---------------------------------------------------------------------------

void TBatchPredictionDlg::EnableBatchDialog()
{
	UpperPanel->Enabled = true;
	OKBtn->Enabled = true;
	RunBtn->Enabled = true;
	StopBatchButton->Enabled = false;
	Application->ProcessMessages();

	return;
}
//---------------------------------------------------------------------------

void __fastcall TBatchPredictionDlg::RunBtnClick(TObject *Sender)
{

	if (FileListBox->Items->Count<1)
	{
		MessageDlg("Please select at least one file!", mtInformation, TMsgDlgButtons() << mbOK, 0);
		return;
	}

	if (!DoSDPredictionsCheckBox->Checked && !DoITPredictionsCheckBox->Checked)
	{
		MessageDlg("Please select at least one GUTS model to predict!", mtInformation, TMsgDlgButtons() << mbOK, 0);
		return;
	}


	// Prepare parameter samples for CI calculations
	// Check if all user input on sample trimming is correct
	// If correct: vSD and vIT contain the requested parameter sample lengths for SD and IT
	int vSD = 0;   // assume no CI: i.e. sample-size is 0
	int vIT = 0;   // assume no CI: i.e. sample-size is 0
	// delete all reporting information from previous batch runs.
	MainForm->LPxBatch_Samplesize_Info_out.resize(0);
	if (LPxCalcCIBatchCheckBox->Checked) {
	   MainForm->LPxBatch_Samplesize_Info_out.push_back("LP50 confidence interval calculation");
		if (DoSDPredictionsCheckBox->Checked)
		{
			vSD = evaluateMaskEdit(MaskEditSDSamplesize, TrackBarSDSamplesize);
			if (vSD < 0)
			{
				ShowMessage("GUTS-RED-SD: Parameter sample size input invalid. Please correct.");
				return;
			}
			else
			{
				MainForm->LPxBatch_Samplesize_Info_out.push_back("   GUTS-RED-SD parameter sample size: " + IntToStr(vSD));
				if (vSD < int(TrackBarSDSamplesize->Max)) {
					MainForm->LPxBatch_Samplesize_Info_out.back() += "; full sample size: " + IntToStr(TrackBarSDSamplesize->Max);
					MainForm->LPxBatch_Samplesize_Info_out.push_back("     CI estimates are based on a sub-sample and might be underestimated.");
				}
			}
		}
		else
		{
			// no SD projections: thus sample size is 0
			vSD = 0;
		}
		if (DoITPredictionsCheckBox->Checked)
		{
			vIT = evaluateMaskEdit(MaskEditITSamplesize, TrackBarITSamplesize);
			if (vIT < 0)
			{
				ShowMessage("GUTS-RED-IT: Parameter sample size input invalid. Please correct.");
				return;
			}
			else
			{
				MainForm->LPxBatch_Samplesize_Info_out.push_back("   GUTS-RED-IT parameter sample size: " + IntToStr(vIT));
				if (vIT < int(TrackBarITSamplesize->Max)) {
					MainForm->LPxBatch_Samplesize_Info_out.back() += "; full sample size: " + IntToStr(TrackBarITSamplesize->Max);
					MainForm->LPxBatch_Samplesize_Info_out.push_back("     CI estimates are based on a sub-sample and might be underestimated.");
				}
			}
		}
		else
		{
			// no IT projections: thus sample size is 0
			vIT = 0;
		}
	}


	// disable dialog
	DisableBatchDialog();

	MainForm->DoITBatchPredictions = DoITPredictionsCheckBox->Checked;
	MainForm->DoSDBatchPredictions = DoSDPredictionsCheckBox->Checked;

	MainForm->BatchFiles.clear();
	MainForm->BatchOutputFolder = c_str(OutputFolderEdit->Text);
	for (int f=0;f<FileListBox->Items->Count; f++)
		MainForm->BatchFiles.push_back(c_str(FileListBox->Items->Strings[f]));

	BatchProgressBar->Position = 0;
	BatchProgressBar->Visible = true;
	Application->ProcessMessages();

	BatchMemo->Clear();
	BatchMemo->Lines->Add("Starting batch processing ...");
    for (
			vector<AnsiString >::const_iterator cit = MainForm->LPxBatch_Samplesize_Info_out.begin();
			cit != MainForm->LPxBatch_Samplesize_Info_out.end();
			++cit
		)
	{
		BatchMemo->Lines->Add(*cit);
	}
	AnsiString err;

	MainForm->RunBatch(
		trimSample(SDPara, vSD),
		trimSample(ITPara, vIT),
		err
	);

	if (err != "")
	{
		Application->ProcessMessages();
		EnableBatchDialog();
		return;
	}

	Application->ProcessMessages();
	LabelSuccess->Visible = true;
	EnableBatchDialog();

	return;
}
//---------------------------------------------------------------------------

void __fastcall TBatchPredictionDlg::DeleteButtonClick(TObject *Sender)
{
	FileListBox->DeleteSelected();
}
//---------------------------------------------------------------------------

void __fastcall TBatchPredictionDlg::ClearButtonClick(TObject *Sender)
{
	while (FileListBox->Items->Count > 0)
		FileListBox->Items->Delete(0);
}
//---------------------------------------------------------------------------


void __fastcall TBatchPredictionDlg::SpeedButton1Click(TObject *Sender)
{
	if (!SetFolderFileOpenDialog->Execute())
		return;

	OutputFolderEdit->Text = SetFolderFileOpenDialog->Files->Strings[0];
}
//---------------------------------------------------------------------------

void __fastcall TBatchPredictionDlg::DoSDPredictionsCheckBoxClick(TObject *Sender)

{
	MainForm->DoSDBatchPredictions = DoSDPredictionsCheckBox->Checked;
	if (LPxCalcCIBatchCheckBox->Checked && DoSDPredictionsCheckBox->Checked)
	{
		// extract the full sample for the confidence interval setting
		SDPara = update_sample(TrackBarSDSamplesize, MaskEditSDSamplesize, LabelSDMin, LabelSDMax, MainForm->CalibrationParameterSpaceSD);
		showSDSamplesizeInterface(true);
	}
	else
	{
		SDPara.resize(0);
		showSDSamplesizeInterface(false);
	}
}
//---------------------------------------------------------------------------

void __fastcall TBatchPredictionDlg::DoITPredictionsCheckBoxClick(TObject *Sender)

{
	MainForm->DoITBatchPredictions = DoITPredictionsCheckBox->Checked;
	if (LPxCalcCIBatchCheckBox->Checked && DoITPredictionsCheckBox->Checked)
	{
		// extract the full sample for the confidence interval setting
		ITPara = update_sample(TrackBarITSamplesize, MaskEditITSamplesize, LabelITMin, LabelITMax, MainForm->CalibrationParameterSpaceIT);
		showITSamplesizeInterface(true);
	}
	else
	{
		ITPara.resize(0);
		showITSamplesizeInterface(false);
	}
}
//---------------------------------------------------------------------------

void __fastcall TBatchPredictionDlg::StopBatchButtonClick(TObject *Sender)
{
	MainForm->StopBatchPredictions = true;
}
//---------------------------------------------------------------------------

void __fastcall TBatchPredictionDlg::FormShow(TObject *Sender)
{
	BatchProgressBar->Visible = false;
	if (LPxCalcCIBatchCheckBox->Checked && DoSDPredictionsCheckBox->Checked)
	{
		SDPara = update_sample(TrackBarSDSamplesize, MaskEditSDSamplesize, LabelSDMin, LabelSDMax, MainForm->CalibrationParameterSpaceSD);
	}
	if (LPxCalcCIBatchCheckBox->Checked && DoITPredictionsCheckBox->Checked)
	{
		ITPara = update_sample(TrackBarITSamplesize, MaskEditITSamplesize, LabelITMin, LabelITMax, MainForm->CalibrationParameterSpaceIT);
	}
}
//---------------------------------------------------------------------------


void __fastcall TBatchPredictionDlg::LPxCalcCIBatchCheckBoxClick(TObject *Sender)

{
	if (LPxCalcCIBatchCheckBox->Checked && DoSDPredictionsCheckBox->Checked)
	{
		// extract the full sample for the confidence interval setting
		SDPara = update_sample(TrackBarSDSamplesize, MaskEditSDSamplesize, LabelSDMin, LabelSDMax, MainForm->CalibrationParameterSpaceSD);
		showSDSamplesizeInterface(true);
	}
	else
	{
		SDPara.resize(0);
		showSDSamplesizeInterface(false);
	}
	if (LPxCalcCIBatchCheckBox->Checked && DoITPredictionsCheckBox->Checked)
	{
		// extract the full sample for the confidence interval setting
		ITPara = update_sample(TrackBarITSamplesize, MaskEditITSamplesize, LabelITMin, LabelITMax, MainForm->CalibrationParameterSpaceIT);
		showITSamplesizeInterface(true);
	}
	else
	{
		ITPara.resize(0);
		showITSamplesizeInterface(false);
	}
}
//---------------------------------------------------------------------------
void __fastcall TBatchPredictionDlg::TrackBarSDSamplesizeChange(TObject *Sender)
{
	changeSamplesizeTrackBar(TrackBarSDSamplesize, MaskEditSDSamplesize);
}
//---------------------------------------------------------------------------
void __fastcall TBatchPredictionDlg::TrackBarITSamplesizeChange(TObject *Sender)
{
	changeSamplesizeTrackBar(TrackBarITSamplesize, MaskEditITSamplesize);
}
//---------------------------------------------------------------------------
void __fastcall TBatchPredictionDlg::MaskEditSDSamplesizeChange(TObject *Sender)
{
	changeSamplesizeMaskEdit(MaskEditSDSamplesize, TrackBarSDSamplesize);
}
//---------------------------------------------------------------------------
void __fastcall TBatchPredictionDlg::MaskEditITSamplesizeChange(TObject *Sender)
{
	  changeSamplesizeMaskEdit(MaskEditITSamplesize, TrackBarITSamplesize);
}
//---------------------------------------------------------------------------

// local functions
vector<TParameterSpace> update_sample(TTrackBar *TB, TEdit *ME, TLabel *LMin, TLabel *LMax, const vector<TParameterSpace>& PS)
{
	vector<TParameterSpace> confPara = MainForm->ParameterSpaceSubsetForErrorPropagation(PS);
	int slen = confPara.size();
	TB->Enabled = true;
	ME->Enabled = true;

	if (TB->Min > slen)
	{
		// Size of parameter space smaller than expected minimum sample size
		// should hardly happen expect for very accurate parameter estimates
		// adjust the trackbar to the lowest value, but indicate the actual sample size
		// disable ptoential adjustments
		TB->Position = TB->Min;
		TB->Max = TB->Min;
		ME->Text = IntToStr(slen);
		LMin->Caption = "The small sample cannot be shortened.";
		LMax->Caption = "";
		TB->Enabled = false;
		ME->Enabled = false;
	}
	else
	{
		if (TB->Position > slen) {
			// Size of parameter space smaller than expected requested sample size
			// adjust maximum and current value to actual sample size
			TB->Position = TB->Min; // to keep position in range, even if slen < Max
			TB->Max = slen;
			TB->Position = slen;
			ME->Text = IntToStr(slen);
			LMin->Caption = IntToStr(TB->Min);
			LMax->Caption = IntToStr(TB->Max);
		} else {
			// if sample size > Position (expected normal case)
			// adjust the maximum
			TB->Max = slen;
            LMin->Caption = IntToStr(TB->Min);
			LMax->Caption = IntToStr(TB->Max);
		}
	}
	return confPara;
}

void showSamplesizeInterface(TTrackBar *TB, TEdit *ME, TLabel *LMin, TLabel *LMax, TLabel *LSamplesize1, TLabel *LSamplesize2, const bool visible) {
	LSamplesize1->Visible = visible;
	LSamplesize2->Visible = visible;
	LMin->Visible = visible;
	LMax->Visible = visible;
	TB->Visible = visible;
	ME->Visible = visible;
}

void showSDSamplesizeInterface(const bool visible) {
	showSamplesizeInterface(
		BatchPredictionDlg->TrackBarSDSamplesize,
		BatchPredictionDlg->MaskEditSDSamplesize,
		BatchPredictionDlg->LabelSDMin,
		BatchPredictionDlg->LabelSDMax,
		BatchPredictionDlg->LabelSDSamplesize1,
		BatchPredictionDlg->LabelSDSamplesize2,

		visible
	);
}

void showITSamplesizeInterface(const bool visible) {
	showSamplesizeInterface(
		BatchPredictionDlg->TrackBarITSamplesize,
		BatchPredictionDlg->MaskEditITSamplesize,
		BatchPredictionDlg->LabelITMin,
		BatchPredictionDlg->LabelITMax,
        BatchPredictionDlg->LabelITSamplesize1,
		BatchPredictionDlg->LabelITSamplesize2,
		visible
	);
}
void changeSamplesizeTrackBar(TTrackBar *TB, TEdit *ME) {
	ME->Text = IntToStr(TB->Position);
}
void changeSamplesizeMaskEdit(TEdit *ME, TTrackBar *TB) {

	int v= evaluateMaskEdit(ME, TB);
    if (v >= 0)
	{
	   TB->Position = v;
	   ME->Color = clWindow;
	} else {
		ME->Color = clRed;
		if (v == -1)
		{
			ShowMessage("Number required as sample size");
		}
	}
}
//---------------------------------------------------------------------------


vector<TParameterSpace> trimSample(const vector<TParameterSpace >& fullSample, const int size)
{
	if (size < fullSample.size())
	{
		vector<TParameterSpace> sample = fullSample;
		random_shuffle(sample.begin(), sample.end());
		sample.resize(size);
		return sample;
	}
	else
	{
		return fullSample;
	}
}

int evaluateMaskEdit(TEdit *ME, TTrackBar *TB) {
	int v;
	if (!TryStrToInt(ME->Text, v))
	{
		return -1;
	}
	else
	{
		if (v < TB->Min || v >  TB->Max)
		{
		   return -2;
		}
		else
		{
		   return v;
		}
	}
}


