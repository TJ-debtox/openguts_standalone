object SettingsDlg: TSettingsDlg
  Left = 227
  Top = 108
  BorderStyle = bsDialog
  Caption = 'Settings'
  ClientHeight = 103
  ClientWidth = 600
  Color = clBtnFace
  ParentFont = True
  OldCreateOrder = True
  Position = poScreenCenter
  PixelsPerInch = 96
  TextHeight = 13
  object Panel1: TPanel
    Left = 0
    Top = 0
    Width = 600
    Height = 69
    Align = alClient
    BevelOuter = bvNone
    TabOrder = 0
    object Panel4: TPanel
      Left = 0
      Top = 0
      Width = 600
      Height = 69
      Align = alClient
      BevelOuter = bvNone
      BorderWidth = 5
      TabOrder = 0
      DesignSize = (
        600
        69)
      object Label1: TLabel
        Left = 12
        Top = 11
        Width = 105
        Height = 13
        Caption = 'Default output folder:'
      end
      object SpeedButton1: TSpeedButton
        Left = 565
        Top = 7
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
      end
      object Label2: TLabel
        Left = 12
        Top = 38
        Width = 99
        Height = 13
        Caption = 'Path to PDF creator:'
      end
      object SpeedButton2: TSpeedButton
        Left = 565
        Top = 34
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
      end
      object OutputFolderEdit: TEdit
        Left = 123
        Top = 8
        Width = 436
        Height = 21
        Anchors = [akLeft, akTop, akRight]
        TabOrder = 0
      end
      object Edit1: TEdit
        Left = 123
        Top = 35
        Width = 436
        Height = 21
        Anchors = [akLeft, akTop, akRight]
        TabOrder = 1
      end
    end
  end
  object Panel2: TPanel
    Left = 0
    Top = 69
    Width = 600
    Height = 34
    Align = alBottom
    BevelOuter = bvNone
    BorderWidth = 5
    TabOrder = 1
    object OKBtn: TButton
      Left = 12
      Top = 1
      Width = 75
      Height = 25
      Caption = '&Close'
      Default = True
      ModalResult = 1
      TabOrder = 0
    end
  end
end
