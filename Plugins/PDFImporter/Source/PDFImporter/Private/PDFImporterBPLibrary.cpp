// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PDFImporterBPLibrary.h"
#include "PDFImporter.h"
#include "Engine.h"
#include "Developer/DesktopPlatform/Public/IDesktopPlatform.h"
#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"
#include "Editor/MainFrame/Public/Interfaces/IMainFrameModule.h"

UPDFImporterBPLibrary::UPDFImporterBPLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}

void UPDFImporterBPLibrary::OpenPDFDialog(const FString& DefaultPath, EOpenPDFDialogResult& OutputPin, FString& FileName)
{
	if (GEngine) 
	{
		if (GEngine->GameViewport)
		{
			IDesktopPlatform* desktopPlatform = FDesktopPlatformModule::Get();
			if(desktopPlatform)
			{
				TArray<FString> resultTemp;
				bool result = desktopPlatform->OpenFileDialog(
					GEngine->GameViewport->GetWindow()->GetNativeWindow()->GetOSWindowHandle(),
					TEXT("Open PDF Dialog"),
					DefaultPath,
					TEXT(""),
					TEXT("PDF File (.pdf)|*.pdf"),
					EFileDialogFlags::Type::None,
					resultTemp
				);

				if (result) 
				{
					//���΃p�X���΃p�X�ɕϊ�
					FileName = FPaths::ConvertRelativePathToFull(resultTemp[0]);
					UE_LOG(PDFImporter, Log, TEXT("Open PDF Dialog : %s"), *FileName);
					OutputPin = EOpenPDFDialogResult::Successful;
					return;
				}
			}
		}
	}
	UE_LOG(PDFImporter, Log, TEXT("Open PDF Dialog : Cancelled"));
	OutputPin = EOpenPDFDialogResult::Cancelled;
}

void UPDFImporterBPLibrary::OpenPDFDialogMultiple(const FString& DefaultPath, EOpenPDFDialogResult& OutputPin, TArray<FString>& FileNames)
{
	if (GEngine)
	{
		if (GEngine->GameViewport)
		{
			IDesktopPlatform* desktopPlatform = FDesktopPlatformModule::Get();
			if (desktopPlatform)
			{
				bool result = desktopPlatform->OpenFileDialog(
					GEngine->GameViewport->GetWindow()->GetNativeWindow()->GetOSWindowHandle(),
					TEXT("Open PDF Dialog"),
					DefaultPath,
					TEXT(""),
					TEXT("PDF File (.pdf)|*.pdf"),
					EFileDialogFlags::Type::Multiple,
					FileNames
				);

				if (result) 
				{
					//���΃p�X���΃p�X�ɕϊ�
					for (FString& fileName : FileNames)
					{
						fileName = FPaths::ConvertRelativePathToFull(fileName);
						UE_LOG(PDFImporter, Log, TEXT("Open PDF Dialog : %s"), *fileName);
					}
					OutputPin = EOpenPDFDialogResult::Successful;
					return;
				}
			}
		}
	}
	UE_LOG(PDFImporter, Log, TEXT("Open PDF Dialog : Cancelled"));
	OutputPin = EOpenPDFDialogResult::Cancelled;
}

void* UPDFImporterBPLibrary::GetWindowHandle()
{
	//�G�f�B�^�̏ꍇ
	if (GIsEditor)
	{
		IMainFrameModule& MainFrameModule = IMainFrameModule::Get();
		TSharedPtr<SWindow> MainWindow = MainFrameModule.GetParentWindow();

		if (MainWindow.IsValid() && MainWindow->GetNativeWindow().IsValid())
		{
			return MainWindow->GetNativeWindow()->GetOSWindowHandle();
		}
	}
	//���s���̏ꍇ
	else
	{
		if (GEngine && GEngine->GameViewport)
		{
			return GEngine->GameViewport->GetWindow()->GetNativeWindow()->GetOSWindowHandle();
		}
	}

	return nullptr;
}