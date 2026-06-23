
// RealTimeGraph.h: главный файл заголовка для приложения PROJECT_NAME
//

#pragma once

#ifndef __AFXWIN_H__
	#error "включить pch.h до включения этого файла в PCH"
#endif

#include "resource.h"		// основные символы


// CRealTimeGraphApp:
// Сведения о реализации этого класса: RealTimeGraph.cpp
//

class CRealTimeGraphApp : public CWinApp
{
public:
	CRealTimeGraphApp();

// Переопределение
public:
	virtual BOOL InitInstance();

// Реализация

	DECLARE_MESSAGE_MAP()
};

extern CRealTimeGraphApp theApp;
