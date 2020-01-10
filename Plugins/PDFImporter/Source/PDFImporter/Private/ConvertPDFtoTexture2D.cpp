// Fill out your copyright notice in the Description page of Project Settings.

#include "ConvertPDFtoTexture2D.h"
#include "PDFImporter.h"
#include "AsyncExecTask.h"
#include "Engine/Texture2D.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Runtime/Core/Public/HAL/FileManager.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"

UConvertPDFtoTexture2D::UConvertPDFtoTexture2D(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), WorldContextObject(nullptr), bIsActive(false), 
	  PDFFilePath(""), Dpi(0), FirstPage(0), LastPage(0)
{
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);
}

UConvertPDFtoTexture2D* UConvertPDFtoTexture2D::ConvertPDFtoTexture2D(
	const UObject* WorldContextObject, 
	const FString& PDFFilePath, 
	int Dpi,
	int FirstPage,
	int LastPage
){
	UConvertPDFtoTexture2D* Node = NewObject<UConvertPDFtoTexture2D>();
	Node->WorldContextObject = WorldContextObject;
	Node->PDFFilePath = PDFFilePath;
	Node->Dpi = Dpi;
	Node->FirstPage = FirstPage;
	Node->LastPage = LastPage;
	return Node;
}

void UConvertPDFtoTexture2D::Activate()
{
	if (!WorldContextObject)
	{
		FFrame::KismetExecutionMessage(TEXT("Invalid WorldContextObject. Cannot execute ConvertPDFtoTexture2D."), ELogVerbosity::Error);
		return;
	}
	if (bIsActive)
	{
		FFrame::KismetExecutionMessage(TEXT("ConvertPDFtoTexture2D is already running."), ELogVerbosity::Warning);
		return;
	}
	
	//�ϊ��J�n
	auto ConvertTask = new FAutoDeleteAsyncTask<FAsyncExecTask>([this]() { ExecConversion(); });
	ConvertTask->StartBackgroundTask();
}

void UConvertPDFtoTexture2D::ExecConversion()
{
	IFileManager& FileManager = IFileManager::Get();

	//PDF�����邩�m�F
	if (!FileManager.FileExists(*PDFFilePath))
	{
		UE_LOG(PDFImporter, Error, TEXT("File not found : %s"), *PDFFilePath);
		Failed.Broadcast();
	}

	//��Ɨp�̃f�B���N�g�����쐬
	FString TempDirPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ConvertTemp"));
	TempDirPath = FPaths::ConvertRelativePathToFull(TempDirPath);
	if (FileManager.DirectoryExists(*TempDirPath))
	{
		FileManager.DeleteDirectory(*TempDirPath);
	}
	FileManager.MakeDirectory(*TempDirPath);
	UE_LOG(PDFImporter, Log, TEXT("A working directory has been created (%s)"), *TempDirPath);

	//PDF�t�@�C�����R�s�[
	FString ConvertTargetPath = FPaths::Combine(TempDirPath, TEXT("ConvertTarget.pdf"));
	FileManager.Copy(*ConvertTargetPath, *PDFFilePath);
	if (!FileManager.FileExists(*ConvertTargetPath))
	{
		UE_LOG(PDFImporter, Error, TEXT("Couldn't copy pdf file"));
		Failed.Broadcast();
	}

	//Ghostscript��p����PDF����jpg�摜���쐬
	FPDFImporterModule& PDFImporterModule = FModuleManager::GetModuleChecked<FPDFImporterModule>(FName("PDFImporter"));

	FString OutputPath = FPaths::Combine(TempDirPath, TEXT("%10d.jpg"));
	bool bResult = PDFImporterModule.ConvertPdfToJpeg(ConvertTargetPath, OutputPath, Dpi, FirstPage, LastPage);

	//�쐬����jpg�摜��ǂݍ���
	TArray<UTexture2D*> Buffer;
	if (bResult)
	{
		//�摜�̃t�@�C���p�X���擾
		TArray<FString> PageNames;
		FileManager.FindFiles(PageNames, *TempDirPath, L"jpg");

		//�摜�̓ǂݍ���
		UTexture2D* TextureTemp;
		for (FString PageName : PageNames)
		{
			if (LoadTexture2DFromFile(FPaths::Combine(TempDirPath, PageName), TextureTemp))
			{
				Buffer.Add(TextureTemp);
			}
			else
			{
				break;
			}
		}
	}

	//��Ɨp�f�B���N�g�����폜
	if (FileManager.DirectoryExists(*TempDirPath))
	{
		if (FileManager.DeleteDirectory(*TempDirPath, false, true))
		{
			UE_LOG(PDFImporter, Log, TEXT("The directory used for work was successfully deleted (%s)"), *TempDirPath);
		}
		else
		{
			UE_LOG(PDFImporter, Error, TEXT("The directory used for work could not be deleted (%s)"), *TempDirPath);
		}
	}
	
	if (bResult) Completed.Broadcast(Buffer);
	else Failed.Broadcast();
}

bool UConvertPDFtoTexture2D::LoadTexture2DFromFile(const FString& FilePath, UTexture2D* &LoadedTexture)
{
	//�z��ɉ摜��ǂݍ���
	TArray<uint8> RawFileData;
	if (!FFileHelper::LoadFileToArray(RawFileData, *FilePath)) return false;

	if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(RawFileData.GetData(), RawFileData.Num()))
	{
		const TArray<uint8>* UncompressedRawData = NULL;
		if (ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, UncompressedRawData))
		{
			//Texture2D���쐬
			LoadedTexture = nullptr;
			LoadedTexture = UTexture2D::CreateTransient(ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), PF_B8G8R8A8);
			if (!LoadedTexture) return false;

			void* TextureData = LoadedTexture->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
			FMemory::Memcpy(TextureData, UncompressedRawData->GetData(), UncompressedRawData->Num());
			LoadedTexture->PlatformData->Mips[0].BulkData.Unlock();
			LoadedTexture->UpdateResource();
		}
	}

	return true;
}