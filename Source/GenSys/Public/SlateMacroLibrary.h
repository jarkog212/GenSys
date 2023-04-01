#pragma once
#include "Styling/SlateTypes.h"

#define KEY_CHANGE_HANDLER_NUMERIC(out, paramName)\
[](const FText& NewText, ETextCommit::Type InTextCommit)\
{\
	out.paramName = FCString::Atof(*NewText.ToString());\
}

#define KEY_CHANGE_HANDLER_BOOL(out, paramName)\
[](ECheckBoxState InState)\
{\
	out.paramName = InState == ECheckBoxState::Checked;\
}

#define KEY_CHANGE_HANDLER_STRING(out, paramName)\
[](const FText& NewText, ETextCommit::Type InTextCommit)\
{\
	out.paramName = TCHAR_TO_ANSI(*NewText.ToString());\
}

#define ARGUMENT_FIELD_NUMERIC(paramStruct , title, paramName, hint)\
+ SVerticalBox::Slot()\
.AutoHeight()\
.Padding(FMargin(30,5))\
[\
	SNew(SHorizontalBox)\
	+ SHorizontalBox::Slot()\
	.VAlign(VAlign_Top)\
	[\
		SNew(STextBlock)\
		.Text(FText::FromString(#title))\
	]\
	+ SHorizontalBox::Slot()\
	.VAlign(VAlign_Top)\
	[\
		SNew(SEditableTextBox)\
		.OnTextCommitted_Lambda(KEY_CHANGE_HANDLER_NUMERIC(paramStruct, paramName))\
		.HintText(FText::FromString(#hint))\
	]\
]

#define ARGUMENT_FIELD_STRING(paramStruct, title, paramName, hint)\
+ SVerticalBox::Slot()\
.AutoHeight()\
.Padding(FMargin(30,5))\
[\
	SNew(SHorizontalBox)\
	+ SHorizontalBox::Slot()\
	.VAlign(VAlign_Top)\
	[\
		SNew(STextBlock)\
		.Text(FText::FromString(#title))\
	]\
	+ SHorizontalBox::Slot()\
	.VAlign(VAlign_Top)\
	[\
		SNew(SEditableTextBox)\
		.OnTextCommitted_Lambda(KEY_CHANGE_HANDLER_STRING(paramStruct, paramName))\
		.HintText(FText::FromString(#hint))\
	]\
]

#define ARGUMENT_CHECKBOX(paramStruct, title, paramName)\
+ SVerticalBox::Slot()\
.AutoHeight()\
.Padding(FMargin(30,5))\
[\
	SNew(SHorizontalBox)\
	+ SHorizontalBox::Slot()\
	.VAlign(VAlign_Top)\
	[\
		SNew(STextBlock)\
		.Text(FText::FromString(#title))\
	]\
	+ SHorizontalBox::Slot()\
	.VAlign(VAlign_Top)\
	[\
		SNew(SCheckBox).OnCheckStateChanged_Lambda(KEY_CHANGE_HANDLER_BOOL(paramStruct, paramName))\
	]\
]

#define SECTION_TITLE(title)\
+ SVerticalBox::Slot()\
.AutoHeight()\
.Padding(FMargin(5,10))\
[\
	SNew(SHorizontalBox)\
	+ SHorizontalBox::Slot()\
	.VAlign(VAlign_Top)\
	[\
		SNew(STextBlock)\
		.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))\
		.Text(FText::FromString(#title))\
	]\
]