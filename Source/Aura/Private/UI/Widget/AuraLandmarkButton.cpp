// Copyright Druid Mechanics

#include "UI/Widget/AuraLandmarkButton.h"

UAuraLandmarkButton::UAuraLandmarkButton()
{
	OnClicked.AddDynamic(this, &UAuraLandmarkButton::HandleClicked);
}

void UAuraLandmarkButton::HandleClicked()
{
	OnLandmarkClicked.Broadcast(LandmarkId);
}
