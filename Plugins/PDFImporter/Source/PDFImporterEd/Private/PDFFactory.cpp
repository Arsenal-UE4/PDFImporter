#include "PDFFactory.h"
#include "GhostscriptCore.h"
#include "PDF.h"
#include "PDFImportOptions.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "Subsystems/ImportSubsystem.h"
#include "EditorFramework/AssetImportData.h"
#include "Framework/Application/SlateApplication.h"
#include "Editor/MainFrame/Public/Interfaces/IMainFrameModule.h"

#define LOCTEXT_NAMESPACE "PDFFactory"

UPDFFactory::UPDFFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UPDF::StaticClass();
	bEditorImport = true;
	bText = true;
	Formats.Add(TEXT("pdf;PDF File"));

	FPDFImporterModule& PDFImporterModule = FModuleManager::LoadModuleChecked<FPDFImporterModule>(FName("PDFImporter"));
	GhostscriptCore = PDFImporterModule.GetGhostscriptCore();
}

bool UPDFFactory::DoesSupportClass(UClass* Class)
{
	return (Class == UPDF::StaticClass());
}

UClass* UPDFFactory::ResolveSupportedClass()
{
	return UPDF::StaticClass();
}

UObject* UPDFFactory::FactoryCreateFile(
	UClass* InClass,
	UObject* InParent,
	FName InName,
	EObjectFlags Flags,
	const FString& Filename,
	const TCHAR* Parms,
	FFeedbackContext* Warn,
	bool& bOutOperationCanceled
)
{
	TSharedPtr<SPDFImportOptions> Options;
	UPDFImportOptions* Result = NewObject<UPDFImportOptions>();
	ShowImportOptionWindow(Options, Filename, Result);

	if (Options->ShouldImport())
	{
		UPDF* NewPDF = CastChecked<UPDF>(StaticConstructObject_Internal(InClass, InParent, InName, Flags));
		UPDF* LoadedPDF = GhostscriptCore->ConvertPdfToPdfAsset(
			Filename, Result->Dpi, Result->FirstPage, Result->LastPage, Result->Locale, true
		);

		if (LoadedPDF != nullptr)
		{
			NewPDF->PageRange = LoadedPDF->PageRange;
			NewPDF->Dpi = LoadedPDF->Dpi;
			NewPDF->Pages = LoadedPDF->Pages;

			NewPDF->AssetImportData = NewObject<UAssetImportData>();
			NewPDF->AssetImportData->SourceData.Insert({ Filename, IFileManager::Get().GetTimeStamp(*Filename) });
			NewPDF->Filename = Filename;
			NewPDF->TimeStamp = IFileManager::Get().GetTimeStamp(*Filename);
		}

		return NewPDF;
	}
	else
	{
		GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, nullptr);
		return nullptr;
	}
}

bool UPDFFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	UPDF* PDF = Cast<UPDF>(Obj);
	if (PDF && PDF->AssetImportData)
	{
		for (auto SourceFile : PDF->AssetImportData->SourceData.SourceFiles)
		{
			OutFilenames.Add(SourceFile.RelativeFilename);
		}
		
		return true;
	}

	return false;
}

void UPDFFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UPDF* PDF = Cast<UPDF>(Obj);
	if (PDF && ensure(NewReimportPaths.Num() == 1))
	{
		PDF->AssetImportData->SourceData.SourceFiles[0].RelativeFilename = NewReimportPaths[0];
	}
}

EReimportResult::Type UPDFFactory::Reimport(UObject* Obj)
{
	UPDF* PDF = Cast<UPDF>(Obj);
	if (!PDF)
	{
		return EReimportResult::Failed;
	}

	const FString Filename = PDF->AssetImportData->SourceData.SourceFiles[0].RelativeFilename;
	if (!Filename.Len() || IFileManager::Get().FileSize(*Filename) == INDEX_NONE)
	{
		return EReimportResult::Failed;
	}

	EReimportResult::Type Result = EReimportResult::Failed;
	if (UFactory::StaticImportObject(
		PDF->GetClass(), PDF->GetOuter(),
		*PDF->GetName(), RF_Public | RF_Standalone, *Filename, NULL, this))
	{
		if (PDF->GetOuter())
		{
			PDF->GetOuter()->MarkPackageDirty();
		}
		else
		{
			PDF->MarkPackageDirty();
		}

		return EReimportResult::Succeeded;
	}

	return EReimportResult::Failed;
}

void UPDFFactory::ShowImportOptionWindow(TSharedPtr<SPDFImportOptions>& Options, const FString& Filename, UPDFImportOptions* &Result)
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "PDF Import Options"))
		.SizingRule(ESizingRule::Autosized);

	Window->SetContent(
		SAssignNew(Options, SPDFImportOptions)
		.WidgetWindow(Window)
		.ImportOptions(Result)
		.Filename(FText::FromString(Filename))
	);

	TSharedPtr<SWindow> ParentWindow;

	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);
}

#undef LOCTEXT_NAMESPACE
