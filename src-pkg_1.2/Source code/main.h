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

#ifndef mainH
#define mainH

//----------------------------------------------------------------------------

#include <Vcl.ComCtrls.hpp>
#include <Vcl.ExtCtrls.hpp>
#include <Vcl.Buttons.hpp>
#include <Vcl.StdCtrls.hpp>
#include <Vcl.Dialogs.hpp>
#include <Vcl.Menus.hpp>
#include <Vcl.Controls.hpp>
#include <Vcl.Forms.hpp>
#include <Vcl.Graphics.hpp>
#include <System.Classes.hpp>
#include <Winapi.Windows.hpp>
#include <Windows.h>
#include <direct.h>

#include <sysutils.hpp>
#include <System.hpp>
#include <Vcl.ActnList.hpp>
#include <Vcl.ImgList.hpp>
#include <Vcl.StdActns.hpp>
#include <Vcl.ToolWin.hpp>
#include <System.Actions.hpp>
#include <System.ImageList.hpp>
#include <Vcl.Grids.hpp>
#include <VCLTee.Chart.hpp>
#include <VCLTee.Series.hpp>
#include <VclTee.TeeGDIPlus.hpp>
#include <VCLTee.TeEngine.hpp>
#include <VCLTee.TeeProcs.hpp>
#include <JPEG.hpp>
#include <Vcl.Samples.Spin.hpp>
#include <SHDocVw.hpp>
#include <Vcl.OleCtrls.hpp>
#include <Clipbrd.hpp>
#include <utilcls.h>
#include <OleServer.hpp>
#include "Word_2K_SRVR.h"
#include <string>
#include <iostream>
#include <vector>
#include <System.SysUtils.hpp>
#include <fstream>
#include <limits>
#include <brent.hpp>
#include <asa047.hpp>
#include <random>
#include <cstdlib>
#include <cerrno>
#include <stdlib.h>
#include <tlhelp32.h>
#include <psapi.h>

#include <algorithm>

using namespace std;

//----------------------------------------------------------------------------

/***********************************************************
************************ Constants *************************
***********************************************************/

const double EPS = 1e-10;

const UnicodeString SOFTWARE_NAME="openGUTS";
const UnicodeString SOFTWARE_VERSION="1.2";
const UnicodeString PROJECT_FILE_EXT="ogp";
const UnicodeString PROJECT_FILE_VERSION="1.4";
const UnicodeString INPUT_DATA_FILE_EXT="txt";

// Debug information:
const short int debugOUTPUT = 1;

// GUTS survival model:
const short int gutsSDIT_BOTH = 0;
const short int gutsSD = 1;
const short int gutsIT = 2;

// GUTS data types
const short int gutsEXPOSURE = 0;
const short int gutsDAMAGE = 1;
const short int gutsSURVIVAL = 2;

// GUTS parameters:
const short int gutsKD = 0;
const short int gutsMW = 1;
const short int gutsHB = 2;
const short int gutsBW = 3;
const short int gutsFS = 4;
const short int gutsPARAMETER_NO = 5;
string gutsPARAMETERS[gutsPARAMETER_NO] = {
	"kd",		// dominant rate constant
	"mw",		// median of threshold distribution
	"hb",		// background mortality
	"bw",		// killing rate
	"Fs", 		// spread of threshold distribution
};
const short int gutsNOT_FIT_PARAM = 0;          // no, don't fit the parameter
const short int gutsFIT_PARAM = 1;              // yes, fit the parameter
const short int gutsLOG_SCALE_PARAM = 0;        // calibrate parameter on log scale
const short int gutsNORM_SCALE_PARAM = 1;       // calibrate parameter on normal scale

const double HIGH_POS_DOUBLE = 1.0e10;	
const double HIGH_NEG_DOUBLE = -1.0e10;	

// Data type information
const short int gutsCONC_DATA = 0;              // data is concentration data
const short int gutsSURV_DATA = 1;              // data is survival data

// Criteria (chi square critical values) for judging the likelihood ratio:
const short int gutsCRIT_TABLE_NO = 5;
const double gutsCRIT_TABLE[gutsCRIT_TABLE_NO] = {        // Critical values for 1-5 d.f. at alpha=0.05 (0.95 CI)
	3.8415,
	5.9915,
	7.8147,
	9.4877,
	11.07,
};

// Next criterion selects the chi-square values to select the sub-sample to propagate to model predictions
const double gutsCRIT_PROP[2] = {
	3.0649,      // Note: chi2inv(0.92,1)
	4.2179,      // Note: chi2inv(0.96,1)
};

// Define settings for LCx and LPx calculations:
const short int predF_LCX_NO = 3;
const double predF_LCX[predF_LCX_NO] = {        // which x in LCx to calculate
	0.50,
	0.20,
	0.10,
};

const short int predF_LCX_PLOT_NO = 2;
const double predF_LCX_PLOT[predF_LCX_PLOT_NO] = {        // which x in LCx to plot (must be sub-set of F_LCx)
	0.50,
	0.10,
};

const short int predT_LCX_NO = 14;
const int predT_LCX[predT_LCX_NO] = {        	// time points at which to calculate/plot LCx
	1,
	2,
	3,
	4,
	7,
	10,
	14,
	21,
	27,
	28,
	42,
	50,
	100,
	182
};

const double LCX_T_END = 16.0;      			// t_end for LCx plot on the x-axis

const short int predF_LPX_NO = 2;
const double predF_LPX[predF_LPX_NO] = {        // which x in LPx to calculate
	0.10,
	0.50,
};

const short int predS_RANGE_NO = 2;
const double predS_RANGE[predS_RANGE_NO] = {        // survival range for making a survival vs. multiplication plot
	0.05,
	0.999,
};

const int predLEN_S_RANGE = 50;     // number of calculated points in survival vs. multiplication plot

const short int predCALC_SURV_SD_C = 0;
const short int predCALC_SURV_SD_T = 1;

// Make options for the standard simplex optimisations:
const short int optimCRIT_ADD_NO = 8;
const double optimCRIT_ADD[optimCRIT_ADD_NO] = {        // extra on top of chi2-criterion to select ok values for next round
	15,
	7,
	3,
	2,
	1.5,
	1,
	0.5,
	0.25,
};

const short int optimN_TR_NO = 8;
const int optimN_TR[optimN_TR_NO] = {        // number of random parameter tries in round 2 and further, for each ok parameter set
	-1,
	60,
	40,
	30,
	20,
	10,
	10,
	10,
};

const short int optimF_D_NO = 8;
const double optimF_D[optimF_D_NO] = {        // maximum step for random mutations, as factor of initial grid spacing
	-1,
	1,
	0.65,
	0.42,
	0.27,
	0.18,
	0.12,
	0.08,
};
const int optimN_OK = 200;       // number of values that will at least continue to next round

const short int optimFITTED_PARAM_NO = 4;
const short int optimVALUES_NUM_AND_INNER_RIM_NO = 2;
const int optimN_CONF_ALL[optimFITTED_PARAM_NO][optimVALUES_NUM_AND_INNER_RIM_NO] = {  // Stop criterion: minimum number of values within the total joint CI and the inner rim, which is made dependent on number of fitted parameters ...
	{500, 500},
	{2500, 1500},
	{5000, 2000},
	{7000, 3000},
};

const int optimTRIES_NO[gutsPARAMETER_NO] = {     // Number of tries per parameter for the initial grid
	14,
	12,
	8,
	12,
	12,
};

const short int optimREGULAR_OPTIM_FLAG = 1;// do a regular optimisation (create parameter set)
const short int optimUSE_SAVED_SET_FLAG = 2;// use a saved parameter set for optimisation
const short int optimROUGH = 0;             // do rough optimisation
const short int optimSTD = 1;               // do standard optimisation
const double optimROUGH_TOLX = 1e-2;        // X tolerance criterium for rough nelder-mead minimising algorithm
const double optimROUGH_TOLFUN = 1e-2;      // Y tolerance criterium for rough nelder-mead minimising algorithm
const int optimROUGH_MAXFUNEVALS_PER_FITPARAM = 30; // maximum function call criterium for rough nelder-mead minimising algorithm
const double optimSTD_TOLX = 1e-4;        	// X tolerance criterium for standard nelder-mead minimising algorithm
const double optimSTD_TOLFUN = 1e-4;      	// Y tolerance criterium for standard nelder-mead minimising algorithm
const int optimSTD_MAXFUNEVALS_PER_FITPARAM = 200; 	// maximum function call criterium for standard nelder-mead minimising algorithm
const double optimSLOWKIN_CORR = 0.70;     	// minimum correlation coefficient between <kd> and <mw>
const double optimSLOWKIN_PARS = 0.05;      // closeness of <kd> and <mw> to their lower bound (as fraction of total range)
const double optimSLOWKIN_F_MW = 3;         // factor by which to multiply current-highest <mw> to get new upper bound
const double optimSLOWKIN_F_KD = 10;        // factor by which to multiply current-highest <kd> to get new upper bound
const double optimCRIT_ADD_EXTRA = 3;       // continue with sets that are within this value from the MLL (this focusses on parameters within, or close to, the inner rim)
const double optimF_D_EXTRA = 1;            // initial maximum jump size as fraction of the grid spacing
const double optimD_EXTRA = 0.1;            // grid spacing as fraction of the parameter's total range (from joint 95% CI)
const double optimGAP_EXTRA = 0.25;         // the gap distance between profile and sample that triggers resampling
const double optimREAL_BETTER = 0.05;       // difference between new optimum and old optimum (MLL) that leads to printing on screen

// constants for the nelmin function (Nelder-Mead simplex algorithm)
const double nelminROUGH_REQMIN = 1e-8;
const double nelminSTD_REQMIN = 1e-15;
const int nelminKONVGE = 1;
const int nelminKCOUNT = 1000;
const int nelminPARAMETERS[gutsPARAMETER_NO] = {
	1,
	2,
	3,
	4,
	5,
};

// Calibration constants:
const short int MAX_CALIBRATION_DATA_SETS_NO = 5;
const short int calibNOT_USE_EXISTING_PARAM_SAMPLE = 1;
const short int calibUSE_EXISTING_PARAM_SAMPLE = 2;
const double calibROOT_FINDING_TOL = 1.0e-5;
const bool calibSD_DEFAULT_BOOL_FIT[gutsPARAMETER_NO] = {true, true, true, true, false};
const bool calibIT_DEFAULT_BOOL_FIT[gutsPARAMETER_NO] = {true, true, true, false, true};
const bool calibSD_DEFAULT_BOOL_LOG[gutsPARAMETER_NO] = {true, false, false, true, false};
const bool calibIT_DEFAULT_BOOL_LOG[gutsPARAMETER_NO] = {true, false, false, false, true};

// Missing data representation:
const string MISSING_DATA_SIGN = "-";
const short int MISSING_DATA_NUM = -1;

// Plot constants
const int plotSINGLE_CHART_HEIGHT = 250;
const int plotSINGLE_CHART_WIDTH = 250;
const int plotSINGLE_CHART_MARGIN = 0;
const short int plotDO_NOT_PAINT = 0;
const short int plotDO_PAINT = 1;
const double plotT_PLOT[2] = {
	100.0,
	1.0*24.0,
};

//For Report
const short int CALIBRATION = 0;
const short int VALIDATION = 1;
const short int PREDICTION = 2;
const short int BATCH_PREDICTION = 3;
const short int MAX_NO_IMAGES_PER_ROW = 3;
const int FONT_SIZE_A = 18;
const int FONT_SIZE_B = 15;

const UnicodeString report_indications[3] = {"Calibration Report", "Validation Report", "Prediction Report"};

const short int SD = 0;
const short int IT = 1;


// Global simulation parameters
const int TIME_POINTS_PER_DAY = 4 * 24;     // number of time points per day for SD analysis (default every 15 minutes)

// time constants
const string SHORT_MONTHS[12] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
const int DAYS_OF_MONTH[12] = {31,28,31,30,31,30,31,31,30,31,30,31};

// graphics
const TColor CL_CONF_BANDS_GREEN = 0x00ccffcc;
const TColor CL_CONF_BANDS_YELLOW = 0x00aaffff;
const TColor CL_EXPOSURE_BLUE = 0x00ff9999;
const short int imgMETAFILE = 0;
const short int imgBITMAP = 1;

/***********************************************************
************************ Structures ************************
***********************************************************/

struct TParameterMatrix
// this is not really a matrix, but in the matlab prototype the data were saved in a matrix, hence the term matrix is used here
{
	vector<double> Value;				// parameter name
	vector<int> Fit; 	        		// fit (=1) or not fit (=0) parameter
	vector<double> Min;    				// minimum parameter value
	vector<double> Max;    				// maximum parameter value
	vector<int> Scale; 	  				// normal scale (=1) or log scale (=0)
};

struct TResultsMatrix
// this is not really a matrix, but in the matlab prototype the data were saved in a matrix, hence the term matrix is used here
{
	vector<double> BestValue;    		// best parameter value
	vector<double> LowerCI;    			// lower bound of confidence interval
	vector<string> LowerCIWarning;  	// warning flag, e.g. "*" = run into a boundary
	vector<double> UpperCI;    			// upper bound of confidence interval
	vector<string> UpperCIWarning;  	// warning flag, e.g. "*" = run into a boundary
	vector<double> LowerEdgeJoint95;    // edges of the joint 95% cloud
	vector<double> UpperEdgeJoint95;    // edges of the joint 95% cloud
	vector<bool> Broken;                // bool for a broken set (broken CI)
};

struct TGridData 				// measured survival OR measured concentrations
{
	vector<AnsiString> GroupNames;		// vector of test group names (survival and concentration group names have to match each other)
	vector<double> Times;				// vector of measurement time steps
	vector<vector<double> > Values;   	// 2D vector of data Values[col][row] (like in grid->Cells[col][row])
	string TimeUnit;           			// unit of survival and concentration time, e.g. [d]
};

struct TDataSet 				// data set containing measured survival and concentrations used for calibration and validation
{
	AnsiString Description;           	// data description (optional)
	TGridData Survival;             	// grid data with survival values
	TGridData Concentration;          	// grid data with concentration values
	string ConcentrationUnit;          	// unit of concentration, e.g. "mg/L"
	int ControlGroupIndex;          	// index of group considered as control (can be changed via combo box)
};

struct TPreparedTreatmentConc 	// this is a structure to mimick the matlab code of the prototype (since it was desired to write the c++ code as close as possible to the matlab code written by Tjalling)
{
	vector<double> Times;        		// time vector with removed NaNs
	vector<double> Conc;				// vector with concentrations for one treatment only
	vector<double> Slope;  				// slope for concentration calculation in interval
};

struct TPreparedTreatmentSurv 	// this is a structure to mimick the matlab code of the prototype (since it was desired to write the c++ code as close as possible to the matlab code written by Tjalling)
{
	vector<double> Times;        		// time vector with removed NaNs
	vector<double> Survivors; 			// vector with survivors for one treatment only
	vector<double> Deaths; 				// vector with deaths for one treatment only
	vector<double> SurvivalProb; 		// vector with survival probabilities for one treatment only
	vector<double> WilsonScoreMin;		// Wilson score min (survival prob. scale [0..1])
	vector<double> WilsonScoreMax;  	// Wilson score max (survival prob. scale [0..1])
};

struct TPreparedDataContainer 	// this is a structure to mimick the matlab code of the prototype (since it was desired to write the c++ code as close as possible to the matlab code written by Tjalling)
{
	vector<vector<TPreparedTreatmentSurv> > Survival;           // prepared survial data
	vector<vector<TPreparedTreatmentConc> > Concentration;      // prepared concentration data
	vector<vector<double> > TWAConcentration;                   // prepared TWA concentration data
	vector<vector<double> > MaxConcentration;                   // prepared max concentration data
	double BackgroundHazard;                                    // overall background hazard for the data collection
	vector<vector<AnsiString> > GroupNames;                     // group names
};

struct TSimulationOutput    	// this is a structure to mimick the matlab code of the prototype (since it was desired to write the c++ code as close as possible to the matlab code written by Tjalling)
{
	vector<double> Times;               	// time vector for simulation output
	vector<double> SurvivalProb;        	// survival probabilities from simulations
	vector<double> SurvivalProbLowerCI;     // lower CI for survival probability
	vector<double> SurvivalProbUpperCI;     // upper CI for survival probability
	vector<double> ScaledDamage;        	// scaled damage from simulations
	vector<double> ScaledDamageLowerCI;     // lower CI for scaled damage
	vector<double> ScaledDamageUpperCI;     // upper CI for scaled damage
};

struct TParameterSpace 			// labelled 'parameter space' as in the matlab code. This is a parameter set and its likelihood. Later on, many paramter sets are used and their likelihood is calculated. All these sets then represent the parameter space.
{
	vector<double> Parameter;          	// parameter values for kd, mw, hb, bw and Fs
	double Likelihood;          		// log-likelihood from simulation with given parameterisation or profile likelihood
};

struct TCalc_Surv_SD_Params 	// global information needed for setup of the zero finding algorithm brent::zero()
{
	double Crit;
	vector<double> Sc;
	vector<double> Time;
	vector<double> Dw;
	double Fraction_Effect;
	int Calc_Surv_Function;
	vector<double> Parameter_Set;
};

struct TTransfer_Params 		// global information needed for setup of the nelder-mead minimisation algorithm
{
	TParameterMatrix PMat;
	int SDIT;
	TPreparedDataContainer Data;
	int FittingParamNumber;
};

struct TPlottingData
{
	vector<vector<double> > Exposure;
	vector<vector<vector<double> > > Damage;
	vector<vector<vector<double> > > MeasuredSurvival;
	vector<vector<vector<double> > > PredictedSurvival;
	// 1. vector level: data set number
	// 2. vector level: group number
	// 3. vector level: best values (.size()=1 if only best values shall be plotted) plus eventual confidence intervals --> .size()=3)
	// 4. vector level: actual values to plot
};

struct TChartDataPoint
{
	double X;
	double Y;
	double Lower_Y;
	double Upper_Y;
};

struct TChartDataPoints
{
	vector<double> Measured_X;
	vector<double> Measured_Y;
	vector<double> Measured_Lower_Y;
	vector<double> Measured_Upper_Y;
	vector<double> Predicted_X;
	vector<double> Predicted_Y;
	vector<double> Predicted_Lower_Y;
	vector<double> Predicted_Upper_Y;
};

struct TGoodnessOfFit           // goodness of fit per data set
{
	AnsiString R_square;
	AnsiString NRMSE;
	vector<AnsiString> SPPE;    // for each treatment
	AnsiString MLL;
	AnsiString AIC;
};

//----------------------------------------------------------------------------

/***********************************************************
*********************** GUI elements ***********************
***********************************************************/

class TMainForm : public TForm
{
__published:
	TOpenDialog *ProjectFileOpenDialog;
	TSaveDialog *ProjectFileSaveDialog;
	TToolBar *ToolBar1;
	TToolButton *ToolButton9;
	TToolButton *ToolButton1;
	TToolButton *ToolButton3;
	TActionList *ActionList1;
	TAction *acFileNew;
	TAction *acFileOpen;
	TAction *acFileSave;
	TAction *acFileSaveAs;
	TAction *acFileExit;
	TEditCut *EditCut1;
	TEditCopy *EditCopy1;
    TEditPaste *EditPaste1;
	TAction *HelpAbout1;
	TStatusBar *StatusBar;
	TImageList *ImageList1;
	TMainMenu *MainMenu1;
	TMenuItem *File1;
    TMenuItem *FileNewItem;
	TMenuItem *FileOpenItem;
	TMenuItem *FileSaveItem;
	TMenuItem *FileSaveAsItem;
	TMenuItem *N1;
	TMenuItem *FileExitItem;
	TMenuItem *Help1;
	TMenuItem *HelpAboutItem;
	TPageControl *MainPageControl;
	TTabSheet *InputTab;
	TPanel *InputDataLeftPanel;
	TTabSheet *CalibrationTab;
	TTabSheet *PredictionsTab;
	TTabSheet *ValidationTab;
	TPanel *ValidationLeftPanel;
	TPanel *ValidationLeftMiddlePanel;
	TPanel *ValidationLeftLowerPanel;
	TButton *StartValidationButton;
	TPageControl *CalibrationPageControl;
	TTabSheet *CalibrationOutputTab;
	TTabSheet *CalibrationInfoTab;
	TPanel *CalibrationLeftPanel;
	TPanel *Panel24;
	TButton *StartCalibrationButton;
	TCheckBox *ShowExpertSettingsCheckBox;
	TPageControl *InputDataPageControl2;
	TTabSheet *InputInspectDataTab;
	TTabSheet *InputInfoTab;
	TPanel *PredictionsLeftPanel;
	TPanel *PredictionsLeftLowerPanel;
	TButton *StartSimulationButton;
	TPanel *PredictionsLeftUpperPanel;
	TLabel *Label32;
	TPanel *PredictionsExposureProfilePanel;
	TPanel *Panel43;
	TPageControl *PredictionsPageControl;
	TTabSheet *PredictionsOutputTab;
	TTabSheet *PredictionsInfoTab;
	TTabSheet *PredictionsInspectDataTab;
	TCheckBox *ValidationBGMFitCheckBox;
	TPageControl *CalibrationOutputPageControl;
	TTabSheet *CalibrationSDOutputTab;
	TPageControl *CalibrationSDOutputPageControl;
	TTabSheet *CalibrationSDOutputParamSpaceTab;
	TTabSheet *CalibrationSDOutputSurvivalTab;
	TTabSheet *CalibrationSDOutputObsVSPredTab;
	TPageControl *PageControl2;
	TTabSheet *CalibrationSDOutputPredVSObsSurvivorsTab;
	TTabSheet *CalibrationSDOutputPredVSObsDeathsTab;
	TPanel *Panel10;
	TPanel *Panel11;
	TLabel *Label2;
	TMemo *CalibrationSDMemo;
	TTabSheet *CalibrationITOutputTab;
	TPageControl *CalibrationITOutputPageControl;
	TTabSheet *CalibrationITOutputParamSpaceTab;
	TTabSheet *CalibrationITOutputSurvivalTab;
	TTabSheet *CalibrationITOutputObsVSPredTab;
	TPageControl *CalibrationITOutputObsVSPredPageControl;
	TTabSheet *CalibrationITOutputPredVSObsSurvivorsTab;
	TTabSheet *CalibrationITOutputPredVSObsDeathsTab;
	TPanel *Panel20;
	TPanel *Panel31;
	TLabel *Label40;
	TMemo *CalibrationITMemo;
	TCheckBox *PredictionsGutsSDCheckBox;
	TCheckBox *PredictionsGutsITCheckBox;
	TTabSheet *CalibrationSDOutputLCxTab;
	TTabSheet *CalibrationITOutputLCxTab;
	TToolButton *ToolButton7;
	TToolButton *ToolButton8;
	TAction *acFileProjectDescription;
	TCheckBox *FitHBToControlCheckBox;
	TPanel *Panel16;
	TPanel *Panel17;
	TLabel *Label7;
	TPanel *Panel18;
	TPanel *Panel19;
	TLabel *Label10;
	TPanel *Panel23;
	TPanel *Panel34;
	TLabel *Label15;
	TSpeedButton *ValidDataOpenSpeed;
	TSpeedButton *ValidDataSaveSpeed;
	TPanel *Panel35;
	TLabel *Label21;
	TComboBox *ValidControlCombo;
	TRichEdit *ValidationDataDescriptionRichEdit;
	TPageControl *PredictionsOutputPageControl;
	TTabSheet *PredictionsSDOutputTab;
	TTabSheet *PredictionsITOutputTab;
	TPageControl *PredictionsSDOutputPageControl;
	TTabSheet *PredictionsSDOutputSurvivalTab;
	TTabSheet *PredictionsSDOutputMultiplicationFactorTab;
	TPanel *Panel30;
	TMemo *PredictionsSDOutputMemo;
	TPanel *Panel8;
	TLabel *Label27;
	TPageControl *PredictionsITOutputPageControl;
	TTabSheet *PredictionsITOutputSurvivalTab;
	TTabSheet *PredictionsITOutputMultiplicationFactorTab;
	TPanel *Panel29;
	TMemo *PredictionsITOutputMemo;
	TPanel *Panel41;
	TLabel *Label23;
	TPageControl *ValidationPageControl;
	TTabSheet *ValidationInspectDataTab;
	TTabSheet *ValidationOutputTab;
	TPageControl *ValidationOutputPageControl;
	TTabSheet *ValidationSDOutputTab;
	TPageControl *ValidationSDOutputPageControl;
	TTabSheet *ValidationSDOutputSurvivalTab;
	TTabSheet *ValidationSDOutputPredVSObsTab;
	TPanel *Panel47;
	TPanel *Panel48;
	TLabel *Label18;
	TMemo *ValidationSDOutputMemo;
	TTabSheet *ValidationITOutputTab;
	TPageControl *ValidationITOutputPageControl;
	TTabSheet *ValidationITOutputSurvivalTab;
	TTabSheet *ValidationITOutputPredVSObsTab;
	TPanel *Panel28;
	TPanel *Panel37;
	TLabel *Label6;
	TMemo *ValidationITOutputMemo;
	TTabSheet *ValidationInfoTab;
	TSpeedButton *PredDataOpenSpeed;
	TSpeedButton *PredDataSaveSpeed;
	TLabel *Label5;
	TButton *DisplayValidationDataButton;
	TButton *DisplayPredictionsDataButton;
	TPopupMenu *CopyDataAndImagePopupMenu;
	TMenuItem *CopyDataToClipboard;
	TMenuItem *CopyImageToClipboard;
	TStringGrid *PredictionsConcStringGrid;
	TStringGrid *ValidationConcStringGrid;
	TStringGrid *ValidationSurvStringGrid;
	TScrollBox *CalibInspectDataScrollBox;
	TImage *InputInspectDataImage;
	TPageControl *ExpertSettingsPageControl;
	TTabSheet *ExpertSettingsSDTab;
	TTabSheet *ExpertSettingsITTab;
	TPanel *CalibrationExpertParamSettingPanel;
	TLabel *Label11;
	TLabel *Label12;
	TLabel *Label13;
	TLabel *Label14;
	TLabel *SDParamKDLabel1;
	TLabel *SDParamMWLabel1;
	TLabel *SDParamHBLabel1;
	TLabel *SDParamBWLabel1;
	TLabel *Label9;
	TLabel *SDParamFSLabel1;
	TLabel *SDParamKDLabel2;
	TLabel *SDParamMWLabel2;
	TLabel *SDParamHBLabel2;
	TLabel *SDParamBWLabel2;
	TLabel *SDParamFSLabel2;
	TLabel *Label19;
	TCheckBox *SDParamBWFitCheck;
	TCheckBox *SDParamBWLogCheck;
	TCheckBox *SDParamHBFitCheck;
	TCheckBox *SDParamMWLogCheck;
	TCheckBox *SDParamKDFitCheck;
	TCheckBox *SDParamKDLogCheck;
	TCheckBox *SDParamMWFitCheck;
	TCheckBox *SDParamHBLogCheck;
	TEdit *SDParamBWMinEdit;
	TEdit *SDParamBWMaxEdit;
	TEdit *SDParamHBMinEdit;
	TEdit *SDParamHBMaxEdit;
	TEdit *SDParamMWMinEdit;
	TEdit *SDParamMWMaxEdit;
	TEdit *SDParamKDMinEdit;
	TEdit *SDParamKDMaxEdit;
	TEdit *SDParamKDValueEdit;
	TEdit *SDParamMWValueEdit;
	TEdit *SDParamHBValueEdit;
	TEdit *SDParamBWValueEdit;
	TEdit *SDParamFSValueEdit;
	TCheckBox *SDParamFSFitCheck;
	TCheckBox *SDParamFSLogCheck;
	TEdit *SDParamFSMaxEdit;
	TEdit *SDParamFSMinEdit;
	TPanel *Panel74;
	TLabel *Label46;
	TLabel *Label47;
	TLabel *Label48;
	TLabel *Label49;
	TLabel *ITParamKDLabel1;
	TLabel *ITParamMWLabel1;
	TLabel *ITParamHBLabel1;
	TLabel *ITParamBWLabel1;
	TLabel *Label54;
	TLabel *ITParamFSLabel1;
	TLabel *ITParamKDLabel2;
	TLabel *ITParamMWLabel2;
	TLabel *ITParamHBLabel2;
	TLabel *ITParamBWLabel2;
	TLabel *ITParamFSLabel2;
	TLabel *Label61;
	TCheckBox *ITParamBWFitCheck;
	TCheckBox *ITParamBWLogCheck;
	TCheckBox *ITParamHBFitCheck;
	TCheckBox *ITParamMWLogCheck;
	TCheckBox *ITParamKDFitCheck;
	TCheckBox *ITParamKDLogCheck;
	TCheckBox *ITParamMWFitCheck;
	TCheckBox *ITParamHBLogCheck;
	TEdit *ITParamBWMinEdit;
	TEdit *ITParamBWMaxEdit;
	TEdit *ITParamHBMinEdit;
	TEdit *ITParamHBMaxEdit;
	TEdit *ITParamMWMinEdit;
	TEdit *ITParamMWMaxEdit;
	TEdit *ITParamKDMinEdit;
	TEdit *ITParamKDMaxEdit;
	TEdit *ITParamKDValueEdit;
	TEdit *ITParamMWValueEdit;
	TEdit *ITParamHBValueEdit;
	TEdit *ITParamBWValueEdit;
	TEdit *ITParamFSValueEdit;
	TCheckBox *ITParamFSFitCheck;
	TCheckBox *ITParamFSLogCheck;
	TEdit *ITParamFSMaxEdit;
	TEdit *ITParamFSMinEdit;
	TScrollBox *PredInspectDataScrollBox;
	TImage *PredictionsInspectDataImage;
	TPopupMenu *CopyImagePopupMenu;
	TMenuItem *MenuItem1;
	TAction *acBatch;
	TToolButton *ToolButton2;
	TToolButton *ToolButton4;
	TMenuItem *Batchpredictions1;
	TMenuItem *Batchpredictions2;
	TMenuItem *N2;
	TOpenDialog *InputFileOpenDialog;
	TSaveDialog *InputFileSaveDialog;
	TEdit *ProjectNameEdit;
	TLabel *Label51;
	TScrollBox *CalibSDSurvivalScrollBox;
	TScrollBox *CalibITSurvivalScrollBox;
	TScrollBox *CalibrationSDOutputParamScrollBox;
	TImage *CalibrationSDOutputParamImage;
	TScrollBox *CalibrationITOutputParamScrollBox;
	TImage *CalibrationITOutputParamImage;
	TScrollBox *ScrollBox1;
	TImage *CalibrationSDOutputObsVSPredSurvivorsImage;
	TScrollBox *ScrollBox2;
	TImage *CalibrationITOutputObsVSPredSurvivorsImage;
	TScrollBox *ScrollBox3;
	TImage *CalibrationITOutputObsVSPredDeathsImage;
	TScrollBox *ScrollBox4;
	TImage *CalibrationSDOutputObsVSPredDeathsImage;
	TScrollBox *ValidationITOutputSurvivalScrollBox;
	TScrollBox *ValidationSDOutputSurvivalScrollBox;
	TScrollBox *PredictionsSDSurvivalScrollBox;
	TImage *PredictionsSDOutputSurvivalImage;
	TScrollBox *ScrollBox13;
	TScrollBox *PredictionsITSurvivalScrollBox;
	TScrollBox *ScrollBox16;
	TImage *PredictionsITOutputMultiplicationFactorImage;
	TImage *PredictionsITOutputSurvivalImage;
	TImage *PredictionsSDOutputMultiplicationFactorImage;
	TLabel *CalibrationStatusLabel;
	TPageControl *PageControl1;
	TTabSheet *TabSheet1;
	TScrollBox *ScrollBox7;
	TTabSheet *TabSheet2;
	TScrollBox *ScrollBox8;
	TPageControl *PageControl3;
	TTabSheet *TabSheet3;
	TScrollBox *ScrollBox5;
	TTabSheet *TabSheet4;
	TScrollBox *ScrollBox10;
	TScrollBox *ValidInspectDataScrollBox;
	TImage *ValidationInspectDataImage;
	TImage *ValidationSDOutputObsVSPredSurvivorsImage;
	TImage *ValidationSDOutputObsVSPredDeathsImage;
	TImage *ValidationSDOutputSurvivalImage;
	TImage *ValidationITOutputSurvivalImage;
	TImage *ValidationITOutputObsVSPredSurvivorsImage;
	TImage *ValidationITOutputObsVSPredDeathsImage;
	TImage *Image1;
	TImage *Image2;
	TMenuItem *Reprot1;
	TAction *acCreateReport;
	TMenuItem *Createreport1;
	TToolButton *ToolButton5;
	TToolButton *ToolButton6;
	TSaveDialog *SaveReportDialog;
	TButton *AbortCalibrationButton;
	TButton *CopySDParamsButton;
	TButton *CopyITParamsButton;
	TScrollBox *CalibrationITLCxScrollBox;
	TImage *CalibrationITOutputLCxImage;
	TScrollBox *CalibrationSDLCxScrollBox;
	TImage *CalibrationSDOutputLCxImage;
	TPanel *Panel14;
	TPanel *Panel15;
	TPanel *Panel21;
	TComboBox *LCxDaysSDCombo;
	TLabel *Label29;
	TLabel *Label30;
	TComboBox *LCxDaysITCombo;
	TWebBrowser *WebBrowser1;
	TWebBrowser *WebBrowser2;
	TWebBrowser *WebBrowser3;
	TWebBrowser *WebBrowser4;
	TButton *DeriveSDParamRangesFromDataButton;
	TButton *DeriveITParamRangesFromDataButton;
	TProgressBar *PredictionsProgressBar;
	TButton *AbortPredictionsButton;
	TCheckBox *LPxCalcCICheckBox;
	TPageControl *CalibDataSetPageControl;
	TTabSheet *CalibDataTab1;
	TPanel *InputDataLeftClientPanel;
	TPanel *Panel3;
	TPanel *Panel5;
	TLabel *Label1;
	TStringGrid *CalibConcStringGrid1;
	TPanel *Panel4;
	TPanel *Panel6;
	TLabel *Label8;
	TStringGrid *CalibSurvStringGrid1;
	TPanel *Panel9;
	TPanel *Panel12;
	TRichEdit *InputDataDescriptionRichEdit1;
	TPanel *Panel1;
	TLabel *Label3;
	TSpeedButton *CalibDataOpenSpeed1;
	TSpeedButton *CalibDataSaveSpeed1;
	TPanel *Panel13;
	TLabel *Label4;
	TComboBox *CalibControlCombo1;
	TTabSheet *CalibDataTab2;
	TPanel *Panel2;
	TPanel *Panel7;
	TPanel *Panel22;
	TLabel *Label24;
	TStringGrid *CalibConcStringGrid2;
	TPanel *Panel27;
	TPanel *Panel32;
	TLabel *Label25;
	TStringGrid *CalibSurvStringGrid2;
	TPanel *Panel33;
	TPanel *Panel36;
	TRichEdit *InputDataDescriptionRichEdit2;
	TPanel *Panel38;
	TLabel *Label26;
	TSpeedButton *CalibDataOpenSpeed2;
	TSpeedButton *CalibDataSaveSpeed2;
	TPanel *Panel40;
	TLabel *Label31;
	TComboBox *CalibControlCombo2;
	TTabSheet *CalibDataTab3;
	TPanel *Panel42;
	TPanel *Panel44;
	TPanel *Panel45;
	TLabel *Label33;
	TStringGrid *CalibConcStringGrid3;
	TPanel *Panel46;
	TPanel *Panel49;
	TLabel *Label34;
	TStringGrid *CalibSurvStringGrid3;
	TPanel *Panel50;
	TPanel *Panel51;
	TRichEdit *InputDataDescriptionRichEdit3;
	TPanel *Panel52;
	TLabel *Label35;
	TSpeedButton *CalibDataOpenSpeed3;
	TSpeedButton *CalibDataSaveSpeed3;
	TPanel *Panel53;
	TLabel *Label36;
	TComboBox *CalibControlCombo3;
	TTabSheet *CalibDataTab4;
	TPanel *Panel54;
	TPanel *Panel55;
	TPanel *Panel56;
	TLabel *Label37;
	TStringGrid *CalibConcStringGrid4;
	TPanel *Panel57;
	TPanel *Panel58;
	TLabel *Label38;
	TStringGrid *CalibSurvStringGrid4;
	TPanel *Panel59;
	TPanel *Panel60;
	TRichEdit *InputDataDescriptionRichEdit4;
	TPanel *Panel61;
	TLabel *Label39;
	TSpeedButton *CalibDataOpenSpeed4;
	TSpeedButton *CalibDataSaveSpeed4;
	TPanel *Panel62;
	TLabel *Label41;
	TComboBox *CalibControlCombo4;
	TTabSheet *CalibDataTab5;
	TPanel *Panel63;
	TPanel *Panel64;
	TPanel *Panel65;
	TLabel *Label42;
	TStringGrid *CalibConcStringGrid5;
	TPanel *Panel66;
	TPanel *Panel67;
	TLabel *Label43;
	TStringGrid *CalibSurvStringGrid5;
	TPanel *Panel68;
	TPanel *Panel69;
	TRichEdit *InputDataDescriptionRichEdit5;
	TPanel *Panel70;
	TLabel *Label44;
	TSpeedButton *CalibDataOpenSpeed5;
	TSpeedButton *CalibDataSaveSpeed5;
	TPanel *Panel71;
	TLabel *Label45;
	TComboBox *CalibControlCombo5;
	TPanel *InputDataLeftBottomPanel;
	TButton *DisplayCalibrationDataButton;
	TPanel *InputDataTopTPanel;
	TLabel *Label20;
	TLabel *Label22;
	TSpinEdit *InputDataCalibSetNoSpin;
	TEdit *ConcUnitEdit;void __fastcall acFileNewExecute(TObject *Sender);
		void __fastcall acFileOpenExecute(TObject *Sender);
		void __fastcall acFileSaveExecute(TObject *Sender);
		void __fastcall acFileExitExecute(TObject *Sender);
		void __fastcall HelpAbout1Execute(TObject *Sender);
	void __fastcall FormCreate(TObject *Sender);
	void __fastcall ShowExpertSettingsCheckBoxClick(TObject *Sender);
	void __fastcall StartCalibrationButtonClick(TObject *Sender);
	void __fastcall StartSimulationButtonClick(TObject *Sender);
	void __fastcall DisplayCalibrationDataButtonClick(TObject *Sender);
	void __fastcall StartValidationButtonClick(TObject *Sender);
	void __fastcall PredictionsGutsSDCheckBoxClick(TObject *Sender);
	void __fastcall PredictionsGutsITCheckBoxClick(TObject *Sender);
	void __fastcall InputDataCalibSetNoSpinChange(TObject *Sender);
	void __fastcall PredictionsUseExposureProfileRadioButtonClick(TObject *Sender);
	void __fastcall PredictionsUseBatchProcessingRadioButtonClick(TObject *Sender);
	void __fastcall DisplayValidationDataButtonClick(TObject *Sender);
	void __fastcall DisplayPredictionsDataButtonClick(TObject *Sender);
	void __fastcall CopyDataToClipboardClick(TObject *Sender);
	void __fastcall CopyImageToClipboardClick(TObject *Sender);
	void __fastcall ChartxMouseDown(TObject *Sender, TMouseButton Button, TShiftState Shift,
		  int X, int Y);
	void __fastcall CalibrationSDOutputParamImageMouseDown(TObject *Sender, TMouseButton Button,
		  TShiftState Shift, int X, int Y);
	void __fastcall FormDestroy(TObject *Sender);
	void __fastcall SG1KeyDown(TObject *Sender, WORD &Key, TShiftState Shift);
	void __fastcall CalibDataOpenSpeed1Click(TObject *Sender);
	void __fastcall CalibDataSaveSpeed1Click(TObject *Sender);
	void __fastcall CalibDataOpenSpeed2Click(TObject *Sender);
	void __fastcall CalibDataOpenSpeed3Click(TObject *Sender);
	void __fastcall CalibDataOpenSpeed4Click(TObject *Sender);
	void __fastcall CalibDataOpenSpeed5Click(TObject *Sender);
	void __fastcall CalibDataSaveSpeed5Click(TObject *Sender);
	void __fastcall CalibDataSaveSpeed4Click(TObject *Sender);
	void __fastcall CalibDataSaveSpeed3Click(TObject *Sender);
	void __fastcall CalibDataSaveSpeed2Click(TObject *Sender);
	void __fastcall DeriveSDParamRangesFromDataButtonClick(TObject *Sender);
	void __fastcall FitHBToControlCheckBoxClick(TObject *Sender);
	void __fastcall CalibConcStringGrid1MouseMove(TObject *Sender, TShiftState Shift,
		  int X, int Y);
	void __fastcall PredDataOpenSpeedClick(TObject *Sender);
	void __fastcall InputInspectDataImageMouseDown(TObject *Sender, TMouseButton Button,
          TShiftState Shift, int X, int Y);
	void __fastcall acFileProjectDescriptionExecute(TObject *Sender);
	void __fastcall PredictionsInspectDataImageMouseDown(TObject *Sender, TMouseButton Button,
          TShiftState Shift, int X, int Y);
	void __fastcall MainPageControlChanging(TObject *Sender, bool &AllowChange);
	void __fastcall ProjectNameEditExit(TObject *Sender);
	void __fastcall ValidDataOpenSpeedClick(TObject *Sender);
	void __fastcall ValidDataSaveSpeedClick(TObject *Sender);
	void __fastcall ValidationBGMFitCheckBoxClick(TObject *Sender);
	void __fastcall acCreateReportExecute(TObject *Sender);
	void __fastcall acBatchExecute(TObject *Sender);
	void __fastcall FileSaveAsItemClick(TObject *Sender);
	void __fastcall CalibSurvStringGrid1Exit(TObject *Sender);
	void __fastcall CalibSurvStringGrid2Exit(TObject *Sender);
	void __fastcall CalibSurvStringGrid3Exit(TObject *Sender);
	void __fastcall CalibSurvStringGrid4Exit(TObject *Sender);
	void __fastcall CalibSurvStringGrid5Exit(TObject *Sender);
	void __fastcall ValidationSurvStringGridExit(TObject *Sender);
	void __fastcall AbortCalibrationButtonClick(TObject *Sender);
	void __fastcall CopySDParamsButtonClick(TObject *Sender);
	void __fastcall CopyITParamsButtonClick(TObject *Sender);
	void __fastcall CalibConcStringGrid1Exit(TObject *Sender);
	void __fastcall CalibConcStringGrid2Exit(TObject *Sender);
	void __fastcall CalibConcStringGrid3Exit(TObject *Sender);
	void __fastcall CalibConcStringGrid4Exit(TObject *Sender);
	void __fastcall CalibConcStringGrid5Exit(TObject *Sender);
	void __fastcall ValidationConcStringGridExit(TObject *Sender);
	void __fastcall CalibConcStringGrid1KeyPress(TObject *Sender, System::WideChar &Key);
	void __fastcall CalibSurvStringGrid1KeyPress(TObject *Sender, System::WideChar &Key);
	void __fastcall CalibConcStringGrid2KeyPress(TObject *Sender, System::WideChar &Key);
	void __fastcall CalibSurvStringGrid2KeyPress(TObject *Sender, System::WideChar &Key);
	void __fastcall CalibConcStringGrid3KeyPress(TObject *Sender, System::WideChar &Key);
	void __fastcall CalibSurvStringGrid3KeyPress(TObject *Sender, System::WideChar &Key);
	void __fastcall CalibConcStringGrid4KeyPress(TObject *Sender, System::WideChar &Key);
	void __fastcall CalibSurvStringGrid4KeyPress(TObject *Sender, System::WideChar &Key);
	void __fastcall CalibConcStringGrid5KeyPress(TObject *Sender, System::WideChar &Key);
	void __fastcall CalibSurvStringGrid5KeyPress(TObject *Sender, System::WideChar &Key);
	void __fastcall ValidationConcStringGridKeyPress(TObject *Sender, System::WideChar &Key);
	void __fastcall ValidationSurvStringGridKeyPress(TObject *Sender, System::WideChar &Key);
	void __fastcall ConcUnitEditChange(TObject *Sender);
	void __fastcall InputDataDescriptionRichEdit1Change(TObject *Sender);
	void __fastcall CalibConcStringGrid1Click(TObject *Sender);
	void __fastcall CalibSurvStringGrid1Click(TObject *Sender);
	void __fastcall InputDataDescriptionRichEdit2Change(TObject *Sender);
	void __fastcall CalibConcStringGrid2Click(TObject *Sender);
	void __fastcall CalibSurvStringGrid2Click(TObject *Sender);
	void __fastcall InputDataDescriptionRichEdit3Change(TObject *Sender);
	void __fastcall CalibConcStringGrid3Click(TObject *Sender);
	void __fastcall CalibSurvStringGrid3Click(TObject *Sender);
	void __fastcall InputDataDescriptionRichEdit4Change(TObject *Sender);
	void __fastcall CalibConcStringGrid4Click(TObject *Sender);
	void __fastcall CalibSurvStringGrid4Click(TObject *Sender);
	void __fastcall InputDataDescriptionRichEdit5Change(TObject *Sender);
	void __fastcall CalibConcStringGrid5Click(TObject *Sender);
	void __fastcall CalibSurvStringGrid5Click(TObject *Sender);
	void __fastcall CalibControlCombo1Change(TObject *Sender);
	void __fastcall CalibControlCombo2Change(TObject *Sender);
	void __fastcall CalibControlCombo3Change(TObject *Sender);
	void __fastcall CalibControlCombo4Change(TObject *Sender);
	void __fastcall CalibControlCombo5Change(TObject *Sender);
	void __fastcall ITParamHBFitCheckClick(TObject *Sender);
	void __fastcall SDParamHBFitCheckClick(TObject *Sender);
	void __fastcall SDParamKDFitCheckClick(TObject *Sender);
	void __fastcall SDParamMWFitCheckClick(TObject *Sender);
	void __fastcall SDParamBWFitCheckClick(TObject *Sender);
	void __fastcall ITParamKDFitCheckClick(TObject *Sender);
	void __fastcall ITParamMWFitCheckClick(TObject *Sender);
	void __fastcall ITParamFSFitCheckClick(TObject *Sender);
	void __fastcall LCxDaysSDComboChange(TObject *Sender);
	void __fastcall LCxDaysITComboChange(TObject *Sender);
	void __fastcall SDParamKDMinEditChange(TObject *Sender);
	void __fastcall SDParamKDMaxEditChange(TObject *Sender);
	void __fastcall SDParamMWMinEditChange(TObject *Sender);
	void __fastcall SDParamMWMaxEditChange(TObject *Sender);
	void __fastcall SDParamHBMinEditChange(TObject *Sender);
	void __fastcall SDParamHBMaxEditChange(TObject *Sender);
	void __fastcall SDParamBWMinEditChange(TObject *Sender);
	void __fastcall SDParamBWMaxEditChange(TObject *Sender);
	void __fastcall SDParamFSMinEditChange(TObject *Sender);
	void __fastcall SDParamFSMaxEditChange(TObject *Sender);
	void __fastcall ITParamKDMinEditChange(TObject *Sender);
	void __fastcall ITParamKDMaxEditChange(TObject *Sender);
	void __fastcall ITParamMWMinEditChange(TObject *Sender);
	void __fastcall ITParamMWMaxEditChange(TObject *Sender);
	void __fastcall ITParamHBMinEditChange(TObject *Sender);
	void __fastcall ITParamHBMaxEditChange(TObject *Sender);
	void __fastcall ITParamBWMinEditChange(TObject *Sender);
	void __fastcall ITParamBWMaxEditChange(TObject *Sender);
	void __fastcall ITParamFSMinEditChange(TObject *Sender);
	void __fastcall ITParamFSMaxEditChange(TObject *Sender);
	void __fastcall AbortPredictionsButtonClick(TObject *Sender);
	void __fastcall SDParamKDValueEditChange(TObject *Sender);
	void __fastcall SDParamMWValueEditChange(TObject *Sender);
	void __fastcall SDParamHBValueEditChange(TObject *Sender);
	void __fastcall SDParamBWValueEditChange(TObject *Sender);
	void __fastcall ITParamKDValueEditChange(TObject *Sender);
	void __fastcall ITParamMWValueEditChange(TObject *Sender);
	void __fastcall ITParamHBValueEditChange(TObject *Sender);
	void __fastcall ITParamFSValueEditChange(TObject *Sender);
	void __fastcall ValidationGridKeyDown(TObject *Sender, WORD &Key, TShiftState Shift);
	void __fastcall PredictionsGridKeyDown(TObject *Sender, WORD &Key, TShiftState Shift);
	void __fastcall ValidControlComboChange(TObject *Sender);


private:

	// pointer for easier access to GUI elements:
	TLabel* GUTSSDParamLabels1[gutsPARAMETER_NO];                               // parameter labels in experts settings
	TLabel* GUTSSDParamLabels2[gutsPARAMETER_NO];                               // parameter labels in experts settings
	TEdit* GUTSSDParamValueEdits[gutsPARAMETER_NO];                             // parameter value edits in experts settings
	TEdit* GUTSSDParamMinEdits[gutsPARAMETER_NO];                               // parameter minimum edits in experts settings
	TEdit* GUTSSDParamMaxEdits[gutsPARAMETER_NO];                               // parameter maximum edits in experts settings
	TCheckBox* GUTSSDParamLogChecks[gutsPARAMETER_NO];                          // check box for log space fitting of parameters in experts settings
	TCheckBox* GUTSSDParamFitChecks[gutsPARAMETER_NO];                          // check box for fitting of parameters in experts settings
	TLabel* GUTSITParamLabels1[gutsPARAMETER_NO];                               // parameter labels in experts settings
	TLabel* GUTSITParamLabels2[gutsPARAMETER_NO];                               // parameter labels in experts settings
	TEdit* GUTSITParamValueEdits[gutsPARAMETER_NO];                             // parameter value edits in experts settings
	TEdit* GUTSITParamMinEdits[gutsPARAMETER_NO];                               // parameter minimum edits in experts settings
	TEdit* GUTSITParamMaxEdits[gutsPARAMETER_NO];                               // parameter maximum edits in experts settings
	TCheckBox* GUTSITParamLogChecks[gutsPARAMETER_NO];                          // check box for log space fitting of parameters in experts settings
	TCheckBox* GUTSITParamFitChecks[gutsPARAMETER_NO];                          // check box for fitting of parameters in experts settings
	TTabSheet* CalibrationDataSetTabs[MAX_CALIBRATION_DATA_SETS_NO];            // tabs for calibration data sets in input data tab
	TStringGrid* CalibConcStringGrids[MAX_CALIBRATION_DATA_SETS_NO];            // conc grids for calibration data sets in input data tab
	TStringGrid* CalibSurvStringGrids[MAX_CALIBRATION_DATA_SETS_NO];            // surv grids for calibration data sets in input data tab
	TComboBox* CalibControlCombos[MAX_CALIBRATION_DATA_SETS_NO];                // combo box for choosing the control group of each calibration data set in input data tab
	TRichEdit* CalibDataDescriptionRichEdits[MAX_CALIBRATION_DATA_SETS_NO];  // rich edits for the project descriptions of the calibration data sets

	vector<TImage*> CalibrationSDSurvivalImages;
	vector<TImage*> CalibrationITSurvivalImages;


	// ********** VARIABLES: **********

	// remembering variables
	char OldDecimalSeparator;               // system decimal seperator to remember
	int OldMainFormWidth;                   // width of the main window to remember
	int OldMainFormHeight;                  // height of the main window to remember
	int ColInGrid;      					// global variable is needed to store value before a pressed key triggers another function
	int RowInGrid;      					// global variable is needed to store value before a pressed key triggers another function
	TGridRect SelectionInGrid;          	// global variable is needed to store value before a pressed key triggers another function
	int LastCellX;                          // last visited cell with mouse (x-coordinate)
	int LastCellY;                          // last visited cell with mouse (y-coordinate)

	// status and settings variables:
	bool CalibrationDataChecked;
	bool ValidationDataChecked;
	bool PredictionsDataChecked;
	bool ValidationDataPresent;
	bool SDValidationPerformed;
	bool ITValidationPerformed;
	bool PredictionsDataPresent;
	bool SDPredictionsPerformed;
	bool ITPredictionsPerformed;
	bool ProjectSaved;
	bool CalibrationResultsPlotted;
	bool ValidationResultsPlotted;
	bool ExpertSettingsOn;
	bool PrefitHBToControl;
	bool FitHBToValidationControl;
	bool SDUseCalculatedParameterRangesForCalib;
	bool ITUseCalculatedParameterRangesForCalib;
	bool SDUseExistingParameterSample;
	bool ITUseExistingParameterSample;
	int NumberOfCalibrationDataSets;
	int NumberOfParameterSamplesSD;
	int NumberOfParameterSamplesIT;
	double MaximumSurvivalTime;

	string ProjectFileVersion;
	bool AbortCalibration;
	bool AbortPredictions;
	bool ParameterRangesManipulated;
	bool CurrentlyLoadingProjectFile;

	bool Fix_hb_cal; 						// switch of what to do with background hazard.

	// global variables for output/report
	TMetafile* Metafile;            		// global Metafile for painting in Image
	TMetafileCanvas* mfCanvas;      		// global Metafile canvas for painting
	TImage* Image;                  		// global Image
	TPicture *GlobalPicture;
	TMetafile* ReportMetafile;
	AnsiString CalibrationInspectData;
	AnsiString ValidationInspectData;
	AnsiString PredictionsInspectData;
	vector<AnsiString> CalibrationDataFileNames;
	AnsiString ValidationDataFileName;
	AnsiString PredictionsDataFileName;

	// data structures / input data:
	vector<TDataSet> CalibrationData;      			// calibration data
	vector<TDataSet> TestCalibrationData;      		// test calibration data for inspect data plot
	vector<TDataSet> PredictionsData;
	vector<TDataSet> TestPredictionsData;
	TGridData SingleExposureProfile;            	// single exposure profile for predictions
	vector<TGridData> BashExposureProfiles;     	// exposure profiles for predictions batch processing
	vector<TDataSet> ValidationData;      		    // validation data
	vector<TDataSet> TestValidationData;      		// test validation data for inspect data plot
	vector<vector<TParameterSpace> > CalibrationProfileLikelihoodsSD;   // profile likelihoods from calc_proflik
	vector<vector<TParameterSpace> > CalibrationProfileLikelihoodsIT;   // profile likelihoods from calc_proflik
	TParameterMatrix PMatSD;                        // parameter matrix for SD
	TParameterMatrix PMatIT;                        // parameter matrix for IT
	TResultsMatrix PMatPrintSD;                     // results matrix for SD
	TResultsMatrix PMatPrintIT;                     // results matrix for IT
	AnsiString ConcUnit;                			// global unit of concentration used for calibration, validation and predictions

	// output
	TPreparedDataContainer CalibData_All;
	TPreparedDataContainer CalibData_HB;
	double Calib_MLL_SD;
	double Calib_MLL_IT;
	TPreparedDataContainer ValidationData_All;
	TPreparedDataContainer ValidationData_HB;
	TPreparedDataContainer PredictionsData_All;
	TPreparedDataContainer PredictionsData_HB;
	TGoodnessOfFit CalibrationSDGoodnessOfFit;
	TGoodnessOfFit CalibrationITGoodnessOfFit;
	TGoodnessOfFit ValidationSDGoodnessOfFit;
	TGoodnessOfFit ValidationITGoodnessOfFit;
	vector<vector<vector<double> > > PredictedVsObserved_ObservedTimes;
	vector<vector<vector<double> > > PredictedVsObserved_ObservedSurv;
	vector<vector<vector<double> > > PredictedVsObserved_ObservedDeaths;
	vector<vector<vector<double> > > PredictedVsObserved_PredictedSurv;
	vector<vector<vector<double> > > PredictedVsObserved_PredictedSurv_LowerCI;
	vector<vector<vector<double> > > PredictedVsObserved_PredictedSurv_UpperCI;
	vector<vector<vector<double> > > PredictedVsObserved_PredictedTimes;
	vector<vector<vector<double> > > PredictedVsObserved_PredictedDeaths;

	// variables for the nelmin function (Nelder-Mead simplex algorithm)
	int NelminN;            // nelmin input - number of parameters to fit
	double NelminStart[gutsPARAMETER_NO];   // nelmin input - start parameter
	double NelminXmin[gutsPARAMETER_NO];    // nelmin input/output - X value of the optimum
	double NelminStep[gutsPARAMETER_NO];    // nelmin input - step size
	double NelminYnewlo;    // nelmin output - lowest Y value from cost function
	int NelminIcount;       // nelmin output - number of function calls
	int NelminNumres;       // nelmin output - number of restarts
	int NelminIfault;       // nelmin output - error indicator:
												// 0 - no errors detected
												// 1 - REQMIN, N, or KONVGE has an illegal value
												// 2 - iteration terminated because KCOUNT was exceeded without convergence


	// ********** FUNCTIONS: **********

	// general function
	void Reset(bool reset_calibration_input); 				// reset model parameters
	void ResetGUIElements(bool reset_calibration_input); 	// reset GUI elements

	// grid and GUI functions
	void EvaluatePressedKeysInGrid(TStringGrid *grid, WORD &key, TShiftState &shift);   // evaluate the currently pressed keys on the keyboard
	void CopyFromGridToClipboard(TStringGrid *grid);                            // copy data from TStringGrid to clipboard
	void CutFromGridToClipboard(TStringGrid *grid);                             // cut data from TStringGrid and store it in clipboard
	void PasteFromClipboardToGrid(TStringGrid *grid);                           // paste data from clipboard to TStringGrid
	void SelectAllCellsInGrid(TStringGrid *grid);                               // select all cells in TStringGrid
	void DeleteFromGrid(TStringGrid *grid);                                     // delete data from TStringGrid
	void JumpToNextOrEndOfBlockInGrid(TStringGrid *grid, WORD &key);            // jump to next or end of block in TStringGrid
	void GetDataFromGrid(TStringGrid *grid, TGridData &grid_data);              // get data from TStringGrid
	bool FillGridWithGridData(TGridData &grid_data, TStringGrid *grid);         // fills a TStringGrid with data
	void WriteGridDataToOStream(ofstream &ofile, TStringGrid *grid);            // writes grid data to an ostream
	bool CheckAndGetCalibrationData(vector<TDataSet> &data);
	bool CheckAndGetValidationData(vector<TDataSet> &data);
	bool CheckValidationDataSet();
	AnsiString GetDataSetNumber(AnsiString grid_name);                          // get the data set number from the current grid name
	void ShowCellContent(TStringGrid *grid, int x_pos, int y_pos);        		// shows the cell content as grid hint by MouseMove event
	void ClearGrid(TStringGrid *grid);                                          // clears every cell in the grid except the fixed rows and columns
	void UpdateParameterValuesInGUI();                                          // updates the parameter values in the calibration experts settings after calibration
	void SetDefaultHeader(TStringGrid *grid, int data_type);
	bool CheckIfTopLeftCellEdited(TStringGrid *grid, System::WideChar &Key);
	void DisableGUIForCalibration();
	void EnableGUIAfterCalibration();
	void DisableGUIForPredictions();
	void EnableGUIAfterPredictions();
	void DisableGUI();
	void EnableGUI();
	void DisableMainMenu();
	void EnableMainMenu();
	bool ResetCalibrationSettingInGUI();
	void ClearValidationResults();
	void ClearPredictionsResults();
	void ReconstructCalibrationPlots();

	// updates and checks:
	bool CheckCalibrationDataSets();                    	// checks the calibration data sets (availability of control groups will be checked later)
	bool CheckGridData(TStringGrid *grid, int data_type);   // check input data of grids (for concentration data use data_type=gutsCONC_DATA, for survival data use data_type=gutsSURV_DATA)
	bool StringIsDouble(string s);          				// returns false if string is not a floating-point number
	bool StringIsInteger(string s);  	       				// returns false if string is not an integer
	bool CheckConcSurvCompatibility(TStringGrid *conc_grid, TStringGrid *surv_grid);    // returns false if headers of conc and surv grid don't match
	bool CheckCalibControlGroupAvailability();
	bool CheckValidControlGroupAvailability();
	bool CheckGetAndPreparePredictionsData(TStringGrid *grid);
	void UpdateControlGroupCombo(TComboBox *combo, TStringGrid *grid);
	void UpdateBatchProgressBar(TProgressBar *progress_bar, double &position, double step);

	// preparation functions:
	void PreparePredictionsFileData(vector<double> &time_vec, vector<double> &conc_vec);
	void CreateDummySurvivalDataForExposureScenario(vector<TDataSet> &data);

	// file functions:
	bool SaveProjectToFile(string filename);            // save project file
	bool OpenProjectFromFile(string filename);          // open project file
	bool OpenProjectFromFile_v1_0(ifstream &ifile);
	bool OpenProjectFromFile_v1_1(ifstream &ifile);
	bool OpenProjectFromFile_v1_2(ifstream &ifile);
	bool OpenProjectFromFile_v1_3(ifstream &ifile);
    bool OpenProjectFromFile_v1_4(ifstream &ifile);
	bool OpenConcSurvInputDataFile(string filename, TRichEdit *description,
			TStringGrid *surv_grid, TStringGrid *conc_grid, TEdit *conc_unit, TComboBox *combo);
	bool WriteConcSurvInputFileToGUI(ifstream &ifile, TRichEdit *description,
			TStringGrid *surv_grid, TStringGrid *conc_grid, TEdit *conc_unit, TComboBox *combo);
	void WriteConcSurvCheckedDataToGUI(TGridData &data, TStringGrid *grid, int data_type);
	bool WritePredictionsConcDataToGUI(ifstream &ifile, TStringGrid *conc_grid);
    bool WriteBatchPredictionsConcDataToGUI(ifstream &ifile);
	bool SaveConcSurvInputDataFile(string filename, TRichEdit *description,
			TStringGrid *surv_grid, TStringGrid *conc_grid, TEdit *conc_unit);
	bool WritePredictionsDataFileToGrid(string filename, TStringGrid *pred_grid);
	bool GetPredictionsExposureDataFromFile(vector<double> &time_vec,
			vector<double> &conc_vec, string filename);
	void EraseEmptyStrings(vector<string> &sv);

	// string functions:
	vector<string> SplitString(string str, string delimiter);       // splits a string into elements of a vector
	void GetNoCommentLine(ifstream &ifile, string &s);              // getline() with skipping comment lines starting with '#'
	void EraseSpaces(string &s);                                    // erasing all spaces in a string
	string BaseFileName(string filename);
	AnsiString BaseFileName(AnsiString filename);
    string PathWithoutFile(string filename);

	// display and painting functions
	void PaintGUTSGraphs(TPreparedDataContainer &data_all, int number_of_sets, double hb_fix,
			TScrollBox *scrollbox, TImage *batch_img, int stage, int sdit, AnsiString &err);
	void PaintParameterSpacePlot();                                 // painting the parameter space plot
	void PaintSingleParameterSpacePlot(TMetafile *metafile, vector<TChartDataPoint> &best_value,
			vector<TChartDataPoint> &inner_rim, vector<TChartDataPoint> &outer_rim,
			double min_x, double max_x, double min_y, double max_y, AnsiString x_label, AnsiString y_label);
	void PrepareDataForParameterSpacePlot(vector<TParameterSpace> &best_value, vector<TParameterSpace> &inner_rim,
			vector<TParameterSpace> &outer_rim, vector<double> &param_min, vector<double> &param_max, int sdit, AnsiString &error);
	void PaintSingleLikelihoodProfilePlot(TMetafile *metafile, vector<TChartDataPoint> &best_value,
			vector<TChartDataPoint> &inner_rim, vector<TChartDataPoint> &outer_rim,
			vector<TChartDataPoint> &plot_data_profile_likelihood, double min_x, double max_x,
			AnsiString x_label, AnsiString y_label);
	void PaintParameterSpacePlotLegend(TMetafile *metafile);
	void PreparePredictedVsObserved_DeathData(int sdit);
	void DisplayPredictedVsObservedGraph_Deaths(TPreparedDataContainer &data_all,
			TImage *img, int sdit);
	void PaintInputData(TImage *img, int stage);
	void PaintDataSetExposureGraph(TMetafile *exposure_metafile, TGridData &conc, int data_set_iterator,
			double &max_x, double &max_y);
	bool PaintDataSetGraphs(vector<vector<TChartDataPoint> > &measured_plot_data,
			vector<vector<TChartDataPoint> > &predicted_plot_data, int data_set_iterator,
			vector<AnsiString> group_names, TMetafile* metafile, TMetafileCanvas *mf_canvas, int stage, int data_type);
	void PaintDataSetDamageGraph(TMetafile *damage_metafile, TGridData &damage, TGridData &lower_damage,
			TGridData &upper_damage, double &max_x, double &max_y);
	void PaintDataSetSurvivalGraph(TMetafile *survival_metafile, TGridData &surv, double &max_x, double &max_y);
	void SetFirstPlotOption(bool &first_plot, int g);
	void PaintSingleExposureGraph(TMetafile *metafile, string title, vector<TChartDataPoint> &plot_data,
			double max_time, double max_conc, bool draw_x_labels, bool &first_plot);
	void PaintSingleDamageGraph(TMetafile *metafile, vector<TChartDataPoint> &plot_data,
			double max_time, double max_damage, bool draw_x_labels, bool draw_CI, bool &first_plot);
	void PaintSingleSurvivalGraph(TMetafile *metafile, vector<TChartDataPoint> &measured_plot_data,
			vector<TChartDataPoint> &predicted_plot_data, double max_time, double max_surv,
			bool draw_x_labels, bool draw_measured_data, bool draw_predicted_data, bool draw_wilson_score,
			bool draw_CI, bool &first_plot);
	void PaintPredictionVSObservedGraphs(int stage, int sdit);
	void DisplayParameterMatrixInGUI(TParameterMatrix pmat, int survival_model, bool change_value, bool change_range);
	bool DisplayGUTSGraphs(TPreparedDataContainer &data_all, vector<TSimulationOutput> &data_set_out,
			int data_set_index, TImage *img, bool display_exposure, bool display_damage,
			bool display_measured_survival, bool display_predicted_survival, int stage);
	void DeleteOldImages(TScrollBox *scrollbox);
	bool GetMaxXandYForPlottingAxis(vector<vector<TChartDataPoint> > &plot_data, double &max_x, double &max_y);
	void AdjustImageDimensions(TPreparedDataContainer &data_all, TImage *img,
			bool display_exposure, bool display_damage, bool display_measured_survival, bool display_predicted_survival);
	void AdjustMetafileDimensions(TPreparedDataContainer &data_all, int data_set_index, TMetafile *metafile,
			bool display_exposure, bool display_damage, bool display_measured_survival, bool display_predicted_survival);
	bool DataVecsToChartDataPoints(vector<TChartDataPoint> &plot_data, vector<double> &x, vector<double> &y,
			vector<double> &lower_y, vector<double> &upper_y);
	void PreparedDataToChartDataPoints(vector<vector<TChartDataPoint> > &plot_data, TPreparedDataContainer data_all,
			int data_set_index, int data_type);
	void SimulationOutputDataToChartDataPoints(vector<vector<TChartDataPoint> > &plot_data,
			vector<TSimulationOutput> &data_set_out, int data_type);
	void ParameterSpaceToChartDataPoints(vector<TChartDataPoint> &plot_data, vector<TParameterSpace> &coll,
			int parameter_1, int parameter_2);
	void ParameterLikelihoodToChartDataPoints(vector<TChartDataPoint> &plot_data,
			vector<TParameterSpace> &coll, int parameter_1);
	vector<double> GetMaximumTimePointsFromPreparedData(TPreparedDataContainer &data_all);
	void AppendGraphDataToText(AnsiString &graph_data_as_text, vector<vector<TChartDataPoint> > &plot_data,
			AnsiString header, vector<AnsiString> &group_names, bool print_upper_lower);
	void DisplayPredictedVsObservedGraph(TPreparedDataContainer &data_all,
			TImage *img, int sdit);
	void DisplayGoodnessOfFit(TPreparedDataContainer &data_all, TMemo *memo, int sdit, int stage);
	void PaintValidationOutput(int sdit);
	void DisplayInformationAboutLoadedProjectFile();
	void PlotLCx(TImage *img, int sdit, double x_max);
	void PlotLPx(TImage *img, int sdit, bool paint_CI);

	// calculations:
	vector<double> CalcWilsonScore(int n, int survivors);       		// needed for drawing CI of raw survival data in inspect data plot
	vector<double> Cumtrapz(vector<double> &x, vector<double> &y);    	// returns a vector containing cumulative trapezoidal numerical integration data
	vector<double> EquidistantNumbers(double a, double b, int n);		// returns a vector with n numbers of equal spacing in the interval [a; b]
	double MaxNumInVec(vector<double> &in);                             // returns the highest number in a vector

	// modelling functions:
	bool Startgrid();
	bool DoSimulation(vector<double> &output_time, TPreparedTreatmentConc &prep_conc, vector<double> &params,
			TMetafile *metafile, int paint, int sdit, AnsiString &err);
	bool DoCalibration(AnsiString &err);
	bool DoPredictions();
	bool DoPredictionsBatchProcessing();
	bool DeriveParamRangesFromData();
	bool UpdateParameterRangesBasedOnInputData();
	bool LoadParamDataFromGUI(TParameterMatrix &pmat, int sdit);
	void Prepare_Data(TPreparedDataContainer &data_all, TPreparedDataContainer &data_hb, vector<TDataSet> data_set,
			int number_of_sets, bool prefit_hb);
	void Calc_Surv_IT(vector<double> &Sc, double &LCx, double &LPx, vector<double> maxDw,
			vector<double> parameter_set, double t, double Feff, double mf_try);
	void Calc_Surv_IT(vector<double> &Sc, vector<double> maxDw, vector<double> parameter_set);
	void Calc_Surv_IT(vector<double> &Sc, double &LCx, double &LPx, vector<double> maxDw,
			vector<double> parameter_set, double t, double Feff);
	vector<double> Calc_Damage_Linear(vector<double> &t, double kd, TPreparedTreatmentConc ev_ic);
	TSimulationOutput Calc_Model(vector<double> t, vector<double> &parameter_set, TPreparedTreatmentConc &ev_ic, int sdit, AnsiString &err);
	void Calc_Conf(TSimulationOutput &x_out, vector<double> &t, TPreparedTreatmentConc &ev_ic,
			int sdit, double &hb_fix, int flag_conf, AnsiString &err);
	void Calc_Conf_Per_Data_Set(vector<TSimulationOutput> &data_set_out, vector<double> &time,
			vector<TPreparedTreatmentConc> &ev_ic, int sdit, double &hb_fix, int flag_conf, AnsiString &err);
	void Calc_LCx(vector<vector<double> > &LCx,vector<vector<double> > &LC_low, vector<vector<double> > &LC_high,
			double t_end, int sdit, AnsiString &err);
	void Calc_LPx(vector<vector<double> > &LPx, vector<vector<double> > &LP_low, vector<vector<double> > &LP_high,
			vector<TPreparedTreatmentConc> &data_pred, int flag, int sdit, int stage, TMemo *memo,
			vector<AnsiString> &memo_strings, AnsiString &err, const vector<TParameterSpace>& sample = vector<TParameterSpace>());
	void Disp_PMat(TParameterMatrix &pmat, int sdit, TResultsMatrix &pmat_print, TMemo *memo, bool print_results);
	bool Calc_Optim(TResultsMatrix &pmat_print, double &mll, TParameterMatrix &pmat, int sdit, int flag, AnsiString &err);
	void Calc_Parspace(TParameterMatrix &minmax, double &mll, vector<int> &stats,
			TResultsMatrix &pmat_print, TParameterMatrix pmat, int sdit, AnsiString &err);
	void Calc_Proflik(vector<vector<TParameterSpace> > &coll_prof, vector<TParameterSpace> &coll_ok,
			vector<double> &phat, double &mll, vector<vector<TParameterSpace> > &coll_prof_pruned,
			int &flag_short, TParameterMatrix pmat, vector<TParameterSpace> &coll_all, int sdit, TParameterMatrix &bnds_tmp);

	void Test_Proflik(vector<double> &flag_profile, vector<TParameterSpace> &coll_ok, TParameterMatrix &pmat,
			vector<vector<TParameterSpace> > &coll_prof, vector<TParameterSpace> &coll_all, AnsiString &err);
	vector<TParameterSpace> Rand_Mutations(TParameterMatrix &pmat, vector<TParameterSpace> &coll_ok,
			int sdit, TParameterMatrix &bnds_tmp, int n_tr_i, vector<double> &d_grid_i, AnsiString &err);
	void DoAndPaintPredictions(vector<vector<double> > &lpx, vector<vector<double> > &lp_low,
	vector<vector<double> > &lp_high, TPreparedDataContainer data_all, const vector<TParameterSpace>& paraSample, int flag, int sdit, vector<AnsiString> &memo_strings,
	double hb_fix, AnsiString &err);
	bool PredictFromBatchFile(string filename, const vector<TParameterSpace>& SDParaSample, const vector<TParameterSpace>& ITParaSample, AnsiString &err);

	void DetermineMaximumSurvivalTime(const vector<TPreparedTreatmentSurv> &ev_ic);

	//reporting functions
	int KillProcess(AnsiString executable_file);
	bool TerminateMyProcess(DWORD dwProcessId, UINT uExitCode);
	bool WriteReport(TWordApplication *word_application,
			TWordDocument *word_document, string filename);
	bool ConnectAndActivateWordApplicationAndDocument(TWordApplication *word_application,
			TWordDocument *word_document);
	void CreateAndOpenWordFile(TWordApplication* wordapplication,
			TWordDocument* word_document, string filename, AnsiString &error);
	void AddCalibrationToReport(TWordApplication* wordapplication);
	void AddValidationToReport(TWordApplication* wordapplication);
	void AddPredictionToReport(TWordApplication* wordapplication);
	void CloseWordFile(TWordApplication* wordapplication);
	bool ConvertToPDF(string report_file);
	void SetReportFontTitle(TWordApplication* wordapplication,
			TWordDocument *word_document);
	void SetReportFontStandard(TWordApplication* wordapplication,
			TWordDocument *word_document);
	void SetReportFontCaptionOne(TWordApplication* wordapplication,
			TWordDocument *word_document);
	void SetReportFontCaptionTwo(TWordApplication* wordapplication,
			TWordDocument *word_document);
	void SetReportFontBoldUnderlined(TWordApplication* wordapplication,
			TWordDocument *word_document);
	void SetReportFontTable(TWordApplication* wordapplication,
			TWordDocument *word_document);
    void SetReportFontSmall(TWordApplication* wordapplication,
			TWordDocument *word_document);
	void SetReportFontTableCaption(TWordApplication* wordapplication,
			TWordDocument *word_document);
	void SetReportFontFigureCaption(TWordApplication* wordapplication,
			TWordDocument *word_document);
	AnsiString CurrTimeToUnixTime();

	bool CreateBasicInformationReport(TWordApplication* wordapplication,
			TWordDocument *word_document, AnsiString &error);
	bool CreateCalibrationReport(TWordApplication* wordapplication,
			TWordDocument *word_document, AnsiString &error);
	bool CreateValidationReport(TWordApplication* wordapplication,
			TWordDocument *word_document, AnsiString &error);
	bool CreatePredictionReport(TWordApplication* wordapplication,
			TWordDocument *word_document, AnsiString &error);
	bool CreateBatchPredictionReport(TWordApplication* wordapplication,
			TWordDocument *word_document, AnsiString &error);
	bool OpenWordApplicationAndSaveDocument(TWordApplication* wordapplication,
			TWordDocument *word_document, string filename, AnsiString &error);
	bool SaveDocumentAndCloseWordApplication(TWordApplication* wordapplication,
			TWordDocument *word_document, AnsiString &error);
	bool PrepareDocument(TWordApplication* wordapplication, AnsiString &error);
	bool GetEndOfDocument(TWordApplication* wordapplication,
			TWordDocument *word_document, WordRangePtr &end_of_document);
	WordRangePtr AddCaptionToEndOfDocument(TWordApplication* wordapplication,
			TWordDocument *word_document, AnsiString caption, AnsiString &error);
	WordRangePtr AddFigureToEndOfFile(TWordApplication* wordapplication,
			TWordDocument *word_document, TImage* this_image, bool multi_row,
			int image_picture_type, AnsiString &error);
	WordRangePtr AddFigureToEndOfFile(TWordApplication* wordapplication,
			TWordDocument *word_document, TChart* this_chart, AnsiString &error);
	WordRangePtr AddTextFromMemoToEndOfDocument(TWordApplication* wordapplication,
			TWordDocument *word_document, TMemo* this_memo, AnsiString &error);
	WordRangePtr AddTextFromStringVecToEndOfDocument(TWordApplication* wordapplication,
			TWordDocument *word_document, vector<AnsiString> memo_strings, AnsiString &error);
	WordRangePtr AddFigureCaption(TWordApplication* wordapplication,
			TWordDocument *word_document, AnsiString label, AnsiString &error);
	WordRangePtr AddParameterValuesAndCI(TWordApplication* wordapplication,
			TWordDocument* word_document, int SDIT,  AnsiString &error);
	AnsiString MyFloatToStr(double float_num, int significant_digits);
	void AddDataTableToEndOfDocument(TWordApplication* wordapplication,
			TWordDocument *word_document, TGridData &data);
	void AddTableCaption(TWordApplication* wordapplication,
			TWordDocument *word_document, AnsiString caption);
	void AddInputDataToReport(TWordApplication* wordapplication, TWordDocument *word_document,
			vector<TDataSet> &data, int stage, AnsiString &error);
	void AddParagraphToEndOfDocument(TWordApplication* wordapplication,
			TWordDocument *word_document);
	void AddPageBreakToEndOfDocument(TWordApplication* wordapplication,
			TWordDocument *word_document);
	void AddSettingsTableToEndOfDocument(TWordApplication* wordapplication,
			TWordDocument *word_document, TParameterMatrix &pmat);
	void AddGoodnessOfFitToEndOfDocument(TWordApplication* wordapplication,
			TWordDocument *word_document, TGoodnessOfFit &gof, int stage, AnsiString &error);
	void SetupWordDocumentProperties(TWordApplication* wordapplication,
			TWordDocument *word_document);
	void AddTextTableToEndOfDocument(TWordApplication* wordapplication,
		TWordDocument *word_document, vector<AnsiString> &text);


	// other functions:
	void SortParameterSpaceList(vector<TParameterSpace> &param_space);
	double CalcCorrCoef(vector<double> &x, vector<double> &y);
	void __fastcall ImageMouseDown(TObject *Sender, TMouseButton Button, TShiftState Shift, int X, int Y);
	void ToDoAfterInputDataManipulation();
	void ParamEditChange();

	MESSAGE void __fastcall WMGetMinMaxInfo(TWMGetMinMaxInfo &Message);

public:
    BEGIN_MESSAGE_MAP
		VCL_MESSAGE_HANDLER(WM_GETMINMAXINFO, TWMGetMinMaxInfo, WMGetMinMaxInfo)
	END_MESSAGE_MAP(TForm)

	// ********** PUBLIC PARAMETERS: **********
	TCalc_Surv_SD_Params Calc_Surv_SD_Params;
	TTransfer_Params Transfer_Params;

	vector<AnsiString> LCxSD_out;
	vector<AnsiString> LCxIT_out;
	vector<vector<double> > LCxSD;
	vector<vector<double> > LCxSD_low;
	vector<vector<double> > LCxSD_high;
	vector<vector<double> > LCxIT;
	vector<vector<double> > LCxIT_low;
	vector<vector<double> > LCxIT_high;

	vector<vector<double> > LPxSD;
	vector<vector<double> > LPxIT;

	vector<AnsiString> LPxSD_out;
	vector<AnsiString> LPxIT_out;
    vector<AnsiString> LPxBatch_Samplesize_Info_out;
	vector<AnsiString> LPxBatch_out;

    vector<TParameterSpace> CalibrationParameterSpaceSD;     	// parameter space for SD
	vector<TParameterSpace> CalibrationParameterSpaceIT;     	// parameter space for IT

	AnsiString BatchOutputString;

	// batch variables
	vector <string> BatchFiles;
	string CurrentBatchFile;
	string BatchOutputFolder;
	bool DoSDBatchPredictions;
	bool DoITBatchPredictions;
	vector<AnsiString> BatchPredictionsMemoStringsSD;
	vector<AnsiString> BatchPredictionsMemoStringsIT;
	TImage *BatchPredictionSurvivalImageSD;
	TImage *BatchPredictionSurvivalImageIT;
	TImage *BatchPredictionMultiFacImageSD;
	TImage *BatchPredictionMultiFacImageIT;

	bool CalibrationPerformed;
	bool StopBatchPredictions;

    UnicodeString ProjectFileName;
	string ProjectTitle;
	string ProjectDescription;

	// ********** PUBLIC FUNCTIONS: **********
	virtual __fastcall TMainForm(TComponent *AOwner);
	void Calc_Surv_SD_c(double &crit, vector<double> &Sc, double conc, vector<double> parameter_set,
			vector<double> time, double fraction_effect, AnsiString &err);
	void Calc_Surv_SD_c(vector<double> &Sc, double conc, vector<double> parameter_set, vector<double> t, AnsiString &err);
	void Calc_Surv_SD_t(double &crit, vector<double> &Sc, double mf_try, vector<double> parameter_set,
			vector<double> t, vector<double> Dw, double Feff, AnsiString &err);
	void Calc_Surv_SD_t(vector<double> &Sc, double mf_try, vector<double> parameter_set, vector<double> t, vector<double> Dw, AnsiString &err);
	void Setup_Calc_Surv_SD(vector<double> &parameter_set, vector<double> &time, double fraction_effect); 					// setup for Calc_Surv_SD_c
	void Setup_Calc_Surv_SD(vector<double> &parameter_set, vector<double> &time,
			vector<double> &dw, double fraction_effect);  // setup for Calc_Surv_SD_t
	double Transfer(vector<double> &pfit, TParameterMatrix &pmat, int sdit, AnsiString &err);
	double Transfer(vector<double> &pfit, TParameterMatrix &pmat, int sdit, TPreparedDataContainer &datacal_input, AnsiString &err);
	void Setup_Transfer(vector<double> &pfit, TParameterMatrix &pmat, int sdit, TPreparedDataContainer &datacal);
	void KillMSWordProcesses();

	// batch functions
    vector<TParameterSpace> ParameterSpaceSubsetForErrorPropagation(const vector<TParameterSpace> &sample);
	bool RunBatch(const vector<TParameterSpace>& SDParaSample, const vector<TParameterSpace>& ITParaSample, AnsiString &err);
};
// ********** FUNCTIONS OUTSIDE THE CLASS: **********
double Calc_Surv_SD_External(double input_variable);
double Transfer_External(double *x);
string c_str(UnicodeString ustr);
int CountProcesses(AnsiString executable_file);
AnsiString PrintProcessNameAndID( DWORD processID );
AnsiString ERR;
//----------------------------------------------------------------------------
extern TMainForm *MainForm;
//----------------------------------------------------------------------------
#endif
