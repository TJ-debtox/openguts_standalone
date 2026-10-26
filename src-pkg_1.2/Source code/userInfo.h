//---------------------------------------------------------------------------

#ifndef userInfoH
#define userInfoH
//---------------------------------------------------------------------------
#include <System.Classes.hpp>
#include <Vcl.Controls.hpp>
#include <Vcl.StdCtrls.hpp>
#include <Vcl.Forms.hpp>
//---------------------------------------------------------------------------
class TFormUserInfo : public TForm
{
__published:	// Von der IDE verwaltete Komponenten
	TMemo *MemoPad;
private:	// Benutzer-Deklarationen
public:		// Benutzer-Deklarationen
	__fastcall TFormUserInfo(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TFormUserInfo *FormUserInfo;
//---------------------------------------------------------------------------
#endif
