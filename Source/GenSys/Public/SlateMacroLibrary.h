#pragma once

#define ARGUMENT_FIELD(paramName, hint)\
+ SVerticalBox::Slot()\
.AutoHeight()\
.Padding(FMargin(30,5))\
[\
	SNew(SHorizontalBox)\
	+ SHorizontalBox::Slot()\
	.VAlign(VAlign_Top)\
	[\
		SNew(STextBlock)\
		.Text(FText::FromString(#paramName))\
	]\
	+ SHorizontalBox::Slot()\
	.VAlign(VAlign_Top)\
	[\
		SNew(SEditableTextBox)\
		.HintText(FText::FromString(#hint))\
	]\
]

#define ARGUMENT_CHECKBOX(paramName)\
+ SVerticalBox::Slot()\
.AutoHeight()\
.Padding(FMargin(30,5))\
[\
	SNew(SHorizontalBox)\
	+ SHorizontalBox::Slot()\
	.VAlign(VAlign_Top)\
	[\
		SNew(STextBlock)\
		.Text(FText::FromString(#paramName))\
	]\
	+ SHorizontalBox::Slot()\
	.VAlign(VAlign_Top)\
	[\
		SNew(SCheckBox)\
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