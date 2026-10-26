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
#ifndef BatchPrediction_DialogH
#define BatchPrediction_DialogH
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
#include "main.h"
#include <Vcl.ComCtrls.hpp>
#include <Vcl.Mask.hpp>
//----------------------------------------------------------------------------
class TBatchPredictionDlg : public TForm
{
__published:
	TPanel *UpperPanel;
	TPanel *Panel2;
	TButton *RunBtn;
	TButton *OKBtn;
	TPanel *FileAddingPanel;
	TPanel *ListBoxPanel;
	TListBox *FileListBox;
	TButton *AddButton;
	TButton *DeleteButton;
	TButton *ClearButton;
	TOpenDialog *AddFileOpenDialog;
	TPanel *OutputFolderPanel;
	TLabel *Label1;
	TEdit *OutputFolderEdit;
	TSpeedButton *SpeedButton1;
	TFileOpenDialog *SetFolderFileOpenDialog;
	TCheckBox *DoSDPredictionsCheckBox;
	TCheckBox *DoITPredictionsCheckBox;
	TProgressBar *BatchProgressBar;
	TLabel *BatchProgressLabel;
	TButton *StopBatchButton;
	TMemo *BatchMemo;
	TPanel *Panel1;
	TCheckBox *LPxCalcCIBatchCheckBox;
	TTrackBar *TrackBarSDSamplesize;
	TLabel *LabelSDSamplesize1;
	TTrackBar *TrackBarITSamplesize;
	TLabel *LabelITSamplesize1;
	TEdit *MaskEditITSamplesize;
	TEdit *MaskEditSDSamplesize;
	TLabel *LabelSDMin;
	TLabel *LabelITMin;
	TLabel *LabelSDMax;
	TLabel *LabelITMax;
	TLabel *LabelSuccess;
	TLabel *LabelSDSamplesize2;
	TLabel *LabelITSamplesize2;
	void __fastcall FormCreate(TObject *Sender);
	void __fastcall AddButtonClick(TObject *Sender);
	void __fastcall RunBtnClick(TObject *Sender);
	void __fastcall DeleteButtonClick(TObject *Sender);
	void __fastcall ClearButtonClick(TObject *Sender);
	void __fastcall SpeedButton1Click(TObject *Sender);
	void __fastcall DoSDPredictionsCheckBoxClick(TObject *Sender);
	void __fastcall DoITPredictionsCheckBoxClick(TObject *Sender);
	void __fastcall StopBatchButtonClick(TObject *Sender);
	void __fastcall FormShow(TObject *Sender);
	void __fastcall LPxCalcCIBatchCheckBoxClick(TObject *Sender);
	void __fastcall TrackBarSDSamplesizeChange(TObject *Sender);
	void __fastcall MaskEditSDSamplesizeChange(TObject *Sender);
	void __fastcall MaskEditITSamplesizeChange(TObject *Sender);
	void __fastcall TrackBarITSamplesizeChange(TObject *Sender);
private:
	vector<TParameterSpace> SDPara;
	vector<TParameterSpace> ITPara;
public:
	virtual __fastcall TBatchPredictionDlg(TComponent* AOwner);
	void DisableBatchDialog();
	void EnableBatchDialog();
};

// functions
	vector<TParameterSpace> update_sample(TTrackBar *TB, TEdit *ME, TLabel *LMin, TLabel *LMax, const vector<TParameterSpace>& PS);
	void showSamplesizeInterface(TTrackBar *TB, TEdit *ME, TLabel *LMin, TLabel *LMax, TLabel* LSamplesize1, TLabel* LSamplesize2, const bool visible);
	void showSDSamplesizeInterface(const bool visible);
	void showITSamplesizeInterface(const bool visible);
	void changeSamplesizeTrackBar(TTrackBar *TB, TEdit *ME);
	void changeSamplesizeMaskEdit(TEdit *ME, TTrackBar *TB);
    vector<TParameterSpace> trimSample(const vector<TParameterSpace>& fullSample, const int size);
    int evaluateMaskEdit(TEdit *ME, TTrackBar *TB);
//----------------------------------------------------------------------------
extern PACKAGE TBatchPredictionDlg *BatchPredictionDlg;
//----------------------------------------------------------------------------
#endif
