
// BCIController.h: главный файл заголовка для приложения PROJECT_NAME
//

#pragma once

#ifndef __AFXWIN_H__
	#error "включить pch.h до включения этого файла в PCH"
#endif

#include "resource.h"		// основные символы


// CBCIControllerApp:
// Сведения о реализации этого класса: BCIController.cpp
//

class CBCIControllerApp : public CWinApp
{
public:
	CBCIControllerApp();

// Переопределение
public:
	virtual BOOL InitInstance();

// Реализация

	DECLARE_MESSAGE_MAP()
};

extern CBCIControllerApp theApp;
