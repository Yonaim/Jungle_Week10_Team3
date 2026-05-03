#include "GameFramework/ScreenTextActor.h"

#include "Component/TextRenderComponent.h"

IMPLEMENT_CLASS(AScreenTextActor, AActor)

void AScreenTextActor::InitDefaultComponents()
{
	TextRenderComponent = AddComponent<UTextRenderComponent>();
	if (!TextRenderComponent)
	{
		return;
	}

	TextRenderComponent->SetCanDeleteFromDetails(false);
	TextRenderComponent->SetText("Screen Text");
	TextRenderComponent->SetFont(FName("Default"));
	TextRenderComponent->SetRenderSpace(ETextRenderSpace::Screen);
	TextRenderComponent->SetScreenPosition(32.0f, 32.0f);
	TextRenderComponent->SetFontSize(1.0f);
	SetRootComponent(TextRenderComponent);
}
