object BatchPredictionDlg: TBatchPredictionDlg
  Left = 227
  Top = 108
  BorderStyle = bsDialog
  Caption = 'Batch prediction'
  ClientHeight = 601
  ClientWidth = 1178
  Color = clBtnFace
  ParentFont = True
  OldCreateOrder = True
  Position = poScreenCenter
  OnCreate = FormCreate
  OnShow = FormShow
  PixelsPerInch = 96
  TextHeight = 13
  object LabelSuccess: TLabel
    Left = 209
    Top = 369
    Width = 796
    Height = 23
    Caption = 
      'To save the data: please copy the results or close the window an' +
      'd save the report and project.'
    Color = clBtnFace
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clGreen
    Font.Height = -19
    Font.Name = 'Tahoma'
    Font.Style = []
    ParentColor = False
    ParentFont = False
    Visible = False
  end
  object UpperPanel: TPanel
    Left = 0
    Top = 0
    Width = 1178
    Height = 361
    Align = alTop
    BevelOuter = bvNone
    TabOrder = 0
    object OutputFolderPanel: TPanel
      Left = 0
      Top = 255
      Width = 1178
      Height = 106
      Align = alBottom
      BevelOuter = bvNone
      TabOrder = 0
      DesignSize = (
        1178
        106)
      object Label1: TLabel
        Left = 549
        Top = 6
        Width = 69
        Height = 13
        Caption = 'Output folder:'
        Visible = False
      end
      object SpeedButton1: TSpeedButton
        Left = 1112
        Top = 6
        Width = 23
        Height = 22
        Anchors = [akTop, akRight]
        Glyph.Data = {
          F6000000424DF600000000000000760000002800000010000000100000000100
          04000000000080000000C30E0000C30E00001000000000000000000000000000
          8000008000000080800080000000800080008080000080808000C0C0C0000000
          FF0000FF000000FFFF00FF000000FF00FF00FFFF0000FFFFFF00DDDDDDDDDDDD
          DDDDDDDDDDDDDDDDDDDD00000000000DDDDD003333333330DDDD0B0333333333
          0DDD0FB03333333330DD0BFB03333333330D0FBFB000000000000BFBFBFBFB0D
          DDDD0FBFBFBFBF0DDDDD0BFB0000000DDDDDD000DDDDDDDD000DDDDDDDDDDDDD
          D00DDDDDDDDD0DDD0D0DDDDDDDDDD000DDDDDDDDDDDDDDDDDDDD}
        Visible = False
        OnClick = SpeedButton1Click
        ExplicitLeft = 1036
      end
      object LabelSDSamplesize1: TLabel
        Left = 120
        Top = 80
        Width = 55
        Height = 13
        Caption = 'Sample size'
        Visible = False
      end
      object LabelITSamplesize1: TLabel
        Left = 336
        Top = 80
        Width = 55
        Height = 13
        Caption = 'Sample size'
        Visible = False
      end
      object LabelSDMin: TLabel
        Left = 136
        Top = 63
        Width = 18
        Height = 13
        Alignment = taCenter
        Caption = '100'
        Visible = False
      end
      object LabelITMin: TLabel
        Left = 336
        Top = 63
        Width = 18
        Height = 13
        Alignment = taCenter
        Caption = '100'
        Visible = False
      end
      object LabelSDMax: TLabel
        Left = 272
        Top = 63
        Width = 58
        Height = 13
        Alignment = taCenter
        Caption = 'LabelSDMax'
        Visible = False
      end
      object LabelITMax: TLabel
        Left = 472
        Top = 63
        Width = 55
        Height = 13
        Alignment = taCenter
        Caption = 'LabelITMax'
        Visible = False
      end
      object LabelSDSamplesize2: TLabel
        Left = 120
        Top = 90
        Width = 85
        Height = 13
        Caption = 'for calculating CI:'
        Visible = False
      end
      object LabelITSamplesize2: TLabel
        Left = 336
        Top = 90
        Width = 85
        Height = 13
        Caption = 'for calculating CI:'
        Visible = False
      end
      object OutputFolderEdit: TEdit
        Left = 634
        Top = 5
        Width = 446
        Height = 21
        Anchors = [akLeft, akTop, akRight]
        TabOrder = 0
        Visible = False
      end
      object DoSDPredictionsCheckBox: TCheckBox
        Left = 144
        Top = 6
        Width = 169
        Height = 17
        Caption = 'Do GUTS-RED-SD predictions'
        Checked = True
        State = cbChecked
        TabOrder = 1
        OnClick = DoSDPredictionsCheckBoxClick
      end
      object DoITPredictionsCheckBox: TCheckBox
        Left = 343
        Top = 6
        Width = 169
        Height = 17
        Caption = 'Do GUTS-RED-IT predictions'
        Checked = True
        State = cbChecked
        TabOrder = 2
        OnClick = DoITPredictionsCheckBoxClick
      end
      object LPxCalcCIBatchCheckBox: TCheckBox
        Left = 16
        Top = 29
        Width = 97
        Height = 17
        Caption = 'Calc. LPx with CI'
        TabOrder = 3
        OnClick = LPxCalcCIBatchCheckBoxClick
      end
      object TrackBarSDSamplesize: TTrackBar
        Left = 136
        Top = 29
        Width = 170
        Height = 36
        Max = 1000
        Min = 100
        Position = 100
        TabOrder = 4
        Visible = False
        OnChange = TrackBarSDSamplesizeChange
      end
      object TrackBarITSamplesize: TTrackBar
        Left = 336
        Top = 29
        Width = 170
        Height = 36
        Max = 1000
        Min = 100
        Position = 100
        TabOrder = 5
        Visible = False
        OnChange = TrackBarITSamplesizeChange
      end
      object MaskEditITSamplesize: TEdit
        Left = 425
        Top = 82
        Width = 81
        Height = 21
        Alignment = taRightJustify
        TabOrder = 6
        Text = '100'
        Visible = False
        OnChange = MaskEditITSamplesizeChange
      end
      object MaskEditSDSamplesize: TEdit
        Left = 209
        Top = 82
        Width = 81
        Height = 21
        Alignment = taRightJustify
        TabOrder = 7
        Text = '100'
        Visible = False
        OnChange = MaskEditSDSamplesizeChange
      end
    end
    object Panel1: TPanel
      Left = 0
      Top = 0
      Width = 1178
      Height = 255
      Align = alClient
      BevelOuter = bvNone
      TabOrder = 1
      object FileAddingPanel: TPanel
        Left = 1086
        Top = 0
        Width = 92
        Height = 255
        Align = alRight
        BevelOuter = bvNone
        BorderWidth = 5
        TabOrder = 0
        object AddButton: TButton
          Left = 6
          Top = 5
          Width = 75
          Height = 25
          Caption = '&Add file'
          TabOrder = 0
          OnClick = AddButtonClick
        end
        object DeleteButton: TButton
          Left = 6
          Top = 36
          Width = 75
          Height = 25
          Caption = '&Delete file'
          TabOrder = 1
          OnClick = DeleteButtonClick
        end
        object ClearButton: TButton
          Left = 6
          Top = 92
          Width = 75
          Height = 25
          Caption = 'Clea&r'
          TabOrder = 2
          OnClick = ClearButtonClick
        end
      end
      object ListBoxPanel: TPanel
        Left = 0
        Top = 0
        Width = 1086
        Height = 255
        Align = alClient
        BevelOuter = bvNone
        BorderWidth = 5
        TabOrder = 1
        object FileListBox: TListBox
          Left = 5
          Top = 5
          Width = 1076
          Height = 212
          Align = alTop
          ItemHeight = 13
          Items.Strings = (
            'N:\asdasdasd'
            'N:\asdasdasd'
            '')
          TabOrder = 0
        end
      end
    end
  end
  object Panel2: TPanel
    Left = 0
    Top = 400
    Width = 1178
    Height = 40
    Align = alBottom
    BevelOuter = bvNone
    BorderWidth = 5
    TabOrder = 1
    object BatchProgressLabel: TLabel
      Left = 779
      Top = 6
      Width = 42
      Height = 13
      Caption = 'Progress'
    end
    object RunBtn: TButton
      Left = 84
      Top = 1
      Width = 70
      Height = 25
      Cancel = True
      Caption = '&Run batch'
      TabOrder = 0
      OnClick = RunBtnClick
    end
    object OKBtn: TButton
      Left = 5
      Top = 1
      Width = 70
      Height = 25
      Caption = '&Close'
      Default = True
      ModalResult = 1
      TabOrder = 1
    end
    object BatchProgressBar: TProgressBar
      Left = 436
      Top = 6
      Width = 337
      Height = 21
      TabOrder = 2
      Visible = False
    end
    object StopBatchButton: TButton
      Left = 163
      Top = 1
      Width = 70
      Height = 25
      Cancel = True
      Caption = '&Stop'
      Enabled = False
      TabOrder = 3
      OnClick = StopBatchButtonClick
    end
  end
  object BatchMemo: TMemo
    Left = 0
    Top = 440
    Width = 1178
    Height = 161
    Align = alBottom
    Font.Charset = ANSI_CHARSET
    Font.Color = clWindowText
    Font.Height = -13
    Font.Name = 'Courier New'
    Font.Style = []
    ParentFont = False
    ReadOnly = True
    ScrollBars = ssBoth
    TabOrder = 2
  end
  object AddFileOpenDialog: TOpenDialog
    DefaultExt = 'out'
    Filter = 'TOXSWA out-files (*.out)'
    Options = [ofHideReadOnly, ofAllowMultiSelect, ofFileMustExist, ofEnableSizing]
    Left = 328
    Top = 136
  end
  object SetFolderFileOpenDialog: TFileOpenDialog
    FavoriteLinks = <>
    FileTypes = <>
    Options = [fdoPickFolders, fdoPathMustExist]
    Left = 212
    Top = 136
  end
end
