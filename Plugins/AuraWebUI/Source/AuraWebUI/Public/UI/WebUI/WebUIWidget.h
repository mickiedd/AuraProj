// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WebUIWidget.generated.h"

class UWebBrowser;

/**
 * Native UMG host for a local HTML/CSS/JavaScript page.
 *
 * A Blueprint can subclass this widget and provide a named WebBrowser child,
 * or the class creates one at runtime.  The page receives the current bridge
 * URL by replacing __AURA_WEBSOCKET_URL__ before it is loaded.
 */
UCLASS()
class AURAWEBUI_API UWebUIWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category = "Web UI")
	void ReloadWebUI();

	UFUNCTION(BlueprintCallable, Category = "Web UI")
	bool LoadWebUIHtml(const FString& Html);

	UFUNCTION(BlueprintCallable, Category = "Web UI")
	UWebBrowser* GetWebBrowser() const { return WebBrowser; }

	/** Plugin-relative path under the plugin Content directory. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Web UI")
	FString HtmlAssetPath = TEXT("WebUI/index.html");

	/** Allow the browser page to create its local WebSocket connection automatically. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Web UI")
	bool bAutoConnectBridge = true;

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWebBrowser> WebBrowser;

	virtual void NativeOnInitialized() override;

private:
	void EnsureWebBrowser();
	FString LoadHtmlAsset() const;
};
