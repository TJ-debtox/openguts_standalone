object ProjectDescriptionDlg: TProjectDescriptionDlg
  Left = 227
  Top = 108
  BorderStyle = bsDialog
  Caption = 'Project description'
  ClientHeight = 208
  ClientWidth = 581
  Color = clBtnFace
  ParentFont = True
  OldCreateOrder = True
  Position = poScreenCenter
  PixelsPerInch = 96
  TextHeight = 13
  object Panel23: TPanel
    Left = 0
    Top = 0
    Width = 581
    Height = 170
    Align = alClient
    BevelOuter = bvNone
    BorderWidth = 5
    TabOrder = 0
    object ProjectDescriptionRichEdit: TRichEdit
      Left = 5
      Top = 5
      Width = 571
      Height = 160
      Align = alClient
      Font.Charset = ANSI_CHARSET
      Font.Color = clWindowText
      Font.Height = -13
      Font.Name = 'MS Sans Serif'
      Font.Style = []
      ParentFont = False
      ScrollBars = ssVertical
      TabOrder = 0
      Zoom = 100
    end
  end
  object Panel1: TPanel
    Left = 0
    Top = 170
    Width = 581
    Height = 38
    Align = alBottom
    BevelOuter = bvNone
    TabOrder = 1
    object OKBtn: TButton
      Left = 6
      Top = 3
      Width = 75
      Height = 25
      Caption = '&Close'
      Default = True
      ModalResult = 1
      TabOrder = 0
      OnClick = OKBtnClick
    end
  end
end
