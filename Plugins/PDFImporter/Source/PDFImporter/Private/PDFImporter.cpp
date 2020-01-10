// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PDFImporter.h"
#include "Misc/Paths.h"
#include "GenericPlatform/GenericPlatformProcess.h"

#include <string>

#define LOCTEXT_NAMESPACE "FPDFImporterModule"

void FPDFImporterModule::StartupModule()
{
	//dll�t�@�C���̃p�X���擾
	FString APIDllPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("ThirdParty"), TEXT("Ghostscript")));
#ifdef _WIN64
	APIDllPath = FPaths::Combine(APIDllPath, TEXT("Win64"));
#elif _WIN32
	APIDllPath = FPaths::Combine(APIDllPath, TEXT("Win32"));
#endif
	APIDllPath = FPaths::Combine(APIDllPath, TEXT("gsdll.dll"));

	//���W���[�������[�h
	APIModule = FPlatformProcess::GetDllHandle(*APIDllPath);
	if (APIModule == nullptr)
	{
		UE_LOG(PDFImporter, Fatal, TEXT("Failed to load Ghostscript module"));
	}

	//�֐��|�C���^���擾
#pragma warning(push)
#pragma warning( disable : 4191 )
	CreateInstance = (CreateAPIInstance)FPlatformProcess::GetDllExport(APIModule, TEXT("gsapi_new_instance"));
	DeleteInstance = (DeleteAPIInstance)FPlatformProcess::GetDllExport(APIModule, TEXT("gsapi_delete_instance"));
	Init = (InitAPI)FPlatformProcess::GetDllExport(APIModule, TEXT("gsapi_init_with_args"));
	Exit = (ExitAPI)FPlatformProcess::GetDllExport(APIModule, TEXT("gsapi_exit"));
#pragma warning(pop)
	if (CreateInstance == nullptr || DeleteInstance == nullptr || Init == nullptr || Exit == nullptr)
	{
		UE_LOG(PDFImporter, Fatal, TEXT("Failed to get Ghostscript function pointer"));
	}
}

void FPDFImporterModule::ShutdownModule()
{
	FPlatformProcess::FreeDllHandle(APIModule);
}

bool FPDFImporterModule::ConvertPdfToJpeg(const FString& InputPath, const FString& OutputPath, int Dpi, int FirstPage, int LastPage)
{
	if (!(FirstPage > 0 && LastPage > 0 && FirstPage <= LastPage))
	{
		FirstPage = 1;
		LastPage = INT_MAX;
	}

	FString ParamOutputPath(TEXT("-sOutputFile=") + OutputPath);

	const char* Args[20] =
	{
		//Ghostscript���W���o�͂ɏ����o�͂��Ȃ��悤��
		"-q",
		"-dQUIET",

		"-dPARANOIDSAFER",			//�Z�[�t���[�h�Ŏ��s
		"-dBATCH",					//Ghostscript���C���^���N�e�B�u���[�h�ɂȂ�Ȃ��悤��
		"-dNOPAUSE",					//�y�[�W���Ƃ̈ꎞ��~�����Ȃ��悤��
		"-dNOPROMPT",				//�R�}���h�v�����v�g���łȂ��悤��           
		"-dMaxBitmap=500000000",		//�p�t�H�[�}���X�����コ����
		"-dNumRenderingThreads=4",	//�}���`�R�A�Ŏ��s

		//�o�͉摜�̃A���`�G�C���A�X��𑜓x�Ȃ�
		"-dAlignToPixels=0",
		"-dGridFitTT=0",
		"-dTextAlphaBits=4",
		"-dGraphicsAlphaBits=4",

		"-sDEVICE=jpeg",	//jpeg�`���ŏo��
		"-sPAPERSIZE=a7",	//���̃T�C�Y

		"",	// 14 : �n�߂̃y�[�W���w��
		"",	// 15 : �I���̃y�[�W���w��
		"",	// 16 : ����DPI
		"",	// 17 : �c��DPI
		"", // 18 : �o�̓p�X
		""  // 19 : ���̓p�X
	};
	
	std::wstring FirstPage_wstr(*FString(TEXT("-dFirstPage=") + FString::FromInt(FirstPage)));
	std::string FirstPage_str(FirstPage_wstr.begin(), FirstPage_wstr.end());
	Args[14] = FirstPage_str.c_str();

	std::wstring LastPage_wstr(*FString(TEXT("-dLastPage=") + FString::FromInt(LastPage)));
	std::string LastPage_str(LastPage_wstr.begin(), LastPage_wstr.end());
	Args[15] = LastPage_str.c_str();

	std::wstring DpiX_wstr(*FString(TEXT("-dDEVICEXRESOLUTION=") + FString::FromInt(Dpi)));
	std::string DpiX_str(DpiX_wstr.begin(), DpiX_wstr.end());
	Args[16] = DpiX_str.c_str();

	std::wstring DpiY_wstr(*FString(TEXT("-dDEVICEYRESOLUTION=") + FString::FromInt(Dpi)));
	std::string DpiY_str(DpiY_wstr.begin(), DpiY_wstr.end());
	Args[17] = DpiY_str.c_str();

	std::wstring OutputPath_wstr(*FString(TEXT("-sOutputFile=") + OutputPath));
	std::string OutputPath_str(OutputPath_wstr.begin(), OutputPath_wstr.end());
	Args[18] = OutputPath_str.c_str();

	std::wstring InputPath_wstr(*InputPath);
	std::string InputPath_str(InputPath_wstr.begin(), InputPath_wstr.end());
	Args[19] = InputPath_str.c_str();

	//Ghostscript�̃C���X�^���X���쐬
	void* APIInstance = nullptr;
	CreateInstance(&APIInstance, 0);
	if (APIInstance != nullptr)
	{
		//Ghostscript�����s
		int Result = Init(APIInstance, 20, (char**)Args);

		//Ghostscript���I��
		Exit(APIInstance);
		DeleteInstance(APIInstance);

		UE_LOG(PDFImporter, Log, TEXT("Ghostscript Return Code : %d"), Result);

		return Result == 0;
	}

	UE_LOG(PDFImporter, Error, TEXT("Failed to create Ghostscript instance"));
	return false;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FPDFImporterModule, PDFImporter)