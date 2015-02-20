// FoxWriting.cpp : ���� DLL Ӧ�ó���ĵ���������
//

#include "stdafx.h"
#include "FoxWriting.h"
#include "FoxFont.h"
#include "FoxArgs.h"
#include <cmath>
#include <atlbase.h>

#include <d3d8.h>
#pragma comment (lib, "d3d8.lib")
#include <d3dx8.h>
#pragma comment (lib, "d3dx8.lib")

typedef struct _FontNode{
	int index;
	FoxFont * font;
	_FontNode * next;
}FontNode, *PFontNode;

// Ĭ���м��
#define DEFAULT_SEP 0

gm::CGMAPI* gmapi;
FWorkbench workbench;
ULONG_PTR m_gdiplusToken;
INT xdpi, ydpi;
BOOL pixelAlign = FALSE;
INT valign, halign;
PFontNode currentFont = NULL;
int fontCount = 0;
PFontNode fontMap[MAP_SIZE];
float lineSpacing = DEFAULT_SEP;

void SetSprite(PFontTexture t){
	gm::PGMTEXTURE texture = &gmapi->GetTextureArray()[workbench.workingSprite->Subimages[0].GetTextureID()];
	texture->texture = t->texture;
	texture->imageHeight = t->imageHeight;
	texture->imageWidth = t->imageWidth;
	texture->textureHeight = t->textureHeight;
	texture->textureWidth = t->textureWidth;
	texture->isValid = 1;

	if (gm::CGlobals::UseNewStructs()){
		gm::PGMBITMAP_NEW b = &workbench.workingSprite->GetPtr()->structNew.bitmaps[0]->structNew;
		b->width = t->imageWidth;
		b->height = t->imageHeight;
	}
	else {
		gm::PGMSPRITE_OLD b = &workbench.workingSprite->GetPtr()->structOld;
		b->width = t->imageWidth;
		b->height = t->imageHeight;
	}
}

void MeasureString(WCHAR* str, Gdiplus::SizeF* size, DOUBLE sep = DEFAULT_SEP){
	float w = 0, h = 0;

	float lineHMax = currentFont->font->mSizeInWorld;
	float lineW = 0;
	int length = wcslen(str);
	bool rBefore = false;

	for (int i = 0; i < length; i++){
		WCHAR c = str[i];
		if (c == L'\r'){
			rBefore = true;
			h += lineHMax;
			h += sep;
			w = max(w, lineW);
			lineHMax = currentFont->font->mSizeInWorld;
			lineW = 0;
		}
		else if (c == L'\n'){
			if (!rBefore){
				h += lineHMax;
				h += sep;
				w = max(w, lineW);
				lineHMax = currentFont->font->mSizeInWorld;
				lineW = 0;
			}
			rBefore = false;
		}
		else {
			rBefore = false;
			PFontTexture t = currentFont->font->GetCharTexture(c);
			if (t != NULL){
				lineW += t->fontWidth;
				lineHMax = max(lineHMax, t->fontHeight);
			}
		}
	}
	h += lineHMax;
	w = max(w, lineW);

	size->Width = w;
	size->Height = h;
}

void MeasureString(WCHAR* str, Gdiplus::SizeF* size, DOUBLE sep, DOUBLE w){
	if (sep == -1){
		sep = DEFAULT_SEP;
	}
	if (w <= 0){
		MeasureString(str, size, sep);
		return;
	}

	float width = 0, height = 0;

	int length = wcslen(str);

	float currentLineWidth = 0;
	float lineHMax = currentFont->font->mSizeInWorld;
	bool rBefore = false;

	for (int i = 0; i < length; i++){
		WCHAR c = str[i];
		if (c == L'\r'){
			rBefore = true;
			width = max(width, currentLineWidth);
			height += sep;
			height += lineHMax;

			lineHMax = currentFont->font->mSizeInWorld;
			currentLineWidth = 0;
		}
		else if (c == L'\n'){
			if (!rBefore){
				width = max(width, currentLineWidth);
				height += sep;
				height += lineHMax;

				lineHMax = currentFont->font->mSizeInWorld;
				currentLineWidth = 0;
			}
			rBefore = false;
		}
		else {
			rBefore = false;
			PFontTexture t = currentFont->font->GetCharTexture(c);
			if (t != NULL){
				float newWidth = currentLineWidth + t->fontWidth;
				if (newWidth > w){ // ����
					width = max(width, currentLineWidth);
					height += sep;
					height += lineHMax;

					lineHMax = max(currentFont->font->mSizeInWorld, t->fontHeight);
					currentLineWidth = t->fontWidth;
				}
				else{
					currentLineWidth = newWidth;
					lineHMax = max(lineHMax, t->fontHeight);
				}
			}
		}
	}
	width = max(width, currentLineWidth);
	height += lineHMax;

	size->Width = width;
	size->Height = height;
}

FLOAT DrawLine(WCHAR* str, int start, int end, DOUBLE xOrig, DOUBLE yOrig, DOUBLE x, DOUBLE y, DOUBLE xscale, DOUBLE yscale, DOUBLE angle, DOUBLE alpha, FLOAT measuredWidth, int color1, int color2){
	float lineHeight = currentFont->font->mSizeInWorld;

	if (halign == gm::fa_center){
		x -= measuredWidth / 2;
	}
	else if (halign == gm::fa_right){
		x -= measuredWidth;
	}

	for (int i = start; i < end; i++){
		PFontTexture t = currentFont->font->GetCharTexture(str[i]);
		if (t != NULL){
			SetSprite(t);
			double xOffset = x - xOrig - t->fontXOffset + currentFont->font->mXOffset;
			double yOffset = y - yOrig - t->fontYOffset + currentFont->font->mYOffset;
			double rotInRad = angle / 360.0 * M_PI * 2;

			double x0 = xOffset * xscale;
			double y0 = yOffset * yscale;
			xOffset = x0 * cos(rotInRad) + y0 * sin(rotInRad);
			yOffset = x0 * sin(rotInRad) - y0 * cos(rotInRad);
			yOffset = -yOffset;
			//xOffset = -xOffset;
			//gm::sprite_set_offset(workbench.spriteIndex, xOffset, yOffset);
			if (pixelAlign){
				gm::draw_sprite_general(workbench.spriteIndex, 0, 0, 0, t->imageWidth, t->imageHeight, round(xOrig + xOffset), round(yOrig + yOffset), xscale, yscale, angle, color1, color1, color2, color2, alpha);
			}
			else {
				gm::draw_sprite_general(workbench.spriteIndex, 0, 0, 0, t->imageWidth, t->imageHeight, xOrig + xOffset, yOrig + yOffset, xscale, yscale, angle, color1, color1, color2, color2, alpha);
			}
			x += t->fontWidth;
			lineHeight = max(lineHeight, t->fontHeight);
		}
	}

	return lineHeight;
}

inline DOUBLE DrawTextInner(DOUBLE x, DOUBLE y, CONST CHAR* str, DOUBLE w, DOUBLE xscale, DOUBLE yscale, DOUBLE angle, DOUBLE alpha, int color1, int color2){
	DOUBLE xOrig = x;
	DOUBLE yOrig = y;

	USES_CONVERSION;
	LPCWSTR pStr = A2W(str);

	Gdiplus::SizeF size;
	MeasureString((WCHAR*)pStr, &size, lineSpacing, w);

	if (valign == gm::fa_middle){
		y -= size.Height / 2;
	}
	else if (valign == gm::fa_bottom){
		y -= size.Height;
	}

	//int color = gm::draw_get_color();

	int length = wcslen(pStr);

	int i = 0;
	int lineStart = 0;
	float lineWidth = 0;
	bool rBefore = false;
	while (i < length){
		WCHAR c = pStr[i];
		if (c == L'\r'){
			y += DrawLine((WCHAR*)pStr, lineStart, i, xOrig, yOrig, x, y, xscale, yscale, angle, alpha, lineWidth, color1, color2);
			y += lineSpacing;
			i++;
			lineStart = i;
			lineWidth = 0;
			rBefore = true;
		}
		else if (c == L'\n'){
			if (!rBefore){
				y += DrawLine((WCHAR*)pStr, lineStart, i, xOrig, yOrig, x, y, xscale, yscale, angle, alpha, lineWidth, color1, color2);
				y += lineSpacing;
			}
			i++;
			lineStart = i;
			lineWidth = 0;
			rBefore = false;
		}
		else {
			rBefore = false;
			PFontTexture t = currentFont->font->GetCharTexture(c);
			if (t != NULL){
				float newWidth = lineWidth + t->fontWidth;
				if (w > 0 && newWidth > w){
					y += DrawLine((WCHAR*)pStr, lineStart, i, xOrig, yOrig, x, y, xscale, yscale, angle, alpha, lineWidth, color1, color2);
					y += lineSpacing;
					lineStart = i;
					lineWidth = 0;
				}
				else {
					lineWidth = newWidth;
					i++;
				}
			}
		}
	}

	if (i != lineStart){
		DrawLine((WCHAR*)pStr, lineStart, i, xOrig, yOrig, x, y, xscale, yscale, angle, alpha, lineWidth, color1, color2);
	}

	return TRUE;
}

void Restore()
{
	SpriteBackup* bkp = &workbench.sprBackup;
	gm::PGMTEXTURE texture = &gmapi->GetTextureArray()[workbench.workingSprite->Subimages[0].GetTextureID()];
	texture->texture = bkp->backupTexture.texture;
	texture->imageHeight = bkp->backupTexture.imageHeight;
	texture->imageWidth = bkp->backupTexture.imageWidth;
	texture->textureHeight = bkp->backupTexture.textureHeight;
	texture->textureWidth = bkp->backupTexture.textureWidth;
	texture->isValid = bkp->backupTexture.isValid;

	if (gm::CGlobals::UseNewStructs()){
		gm::PGMBITMAP_NEW b = &workbench.workingSprite->GetPtr()->structNew.bitmaps[0]->structNew;
		b->width = bkp->spriteWidth;
		b->height = bkp->spriteHeight;
	}
	else {
		gm::PGMSPRITE_OLD b = &workbench.workingSprite->GetPtr()->structOld;
		b->width = bkp->spriteWidth;
		b->height = bkp->spriteHeight;
	}
}

FOXWRITING_API DOUBLE FWInit(DOUBLE sprite)
{
	int _spr = (int)sprite;
	if (!gmapi->Sprites.Exists(_spr))return FALSE;

	int imageCount = gmapi->Sprites[_spr].Subimages.GetCount();
	if (imageCount <= 0)return FALSE;

	workbench.spriteIndex = _spr;
	workbench.workingSprite = &gmapi->Sprites[_spr];

	// sprite backup doing here
	gm::PGMTEXTURE texture = &gmapi->GetTextureArray()[gmapi->Sprites[_spr].Subimages[0].GetTextureID()];
	memcpy(&workbench.sprBackup.backupTexture, texture, sizeof(gm::GMTEXTURE));
	if (gm::CGlobals::UseNewStructs()){
		gm::PGMBITMAP_NEW b = &workbench.workingSprite->GetPtr()->structNew.bitmaps[0]->structNew;
		workbench.sprBackup.spriteWidth = b->width;
		workbench.sprBackup.spriteHeight = b->height;
	}
	else {
		gm::PGMSPRITE_OLD b = &workbench.workingSprite->GetPtr()->structOld;
		workbench.sprBackup.spriteWidth = b->width;
		workbench.sprBackup.spriteHeight = b->height;
	}

	HRESULT rlt = texture->texture->GetDevice(&workbench.device);
	if (rlt != D3D_OK)
	{
		return FALSE;
	}

	ZeroMemory(fontMap, sizeof(fontMap));

	pixelAlign = FALSE;
	halign = gm::fa_left;
	valign = gm::fa_top;

	return TRUE;
}

FOXWRITING_API DOUBLE FWReleaseCache()
{
	Restore();
	// release all texture
	for (int i = 0; i < MAP_SIZE; i++){
		PFontNode node = fontMap[i];
		while (node != NULL){
			PFontNode next = node->next;

			FoxFont* f = node->font;
			f->FreeCache();
			node = next;
		}
	}
	return TRUE;
}

FOXWRITING_API DOUBLE FWCleanup(){
	Restore();
	currentFont = NULL;

	// cleanup all fonts
	for (int i = 0; i < MAP_SIZE; i++){
		PFontNode node = fontMap[i];
		while (node != NULL){
			PFontNode next = node->next;

			FoxFont* f = node->font;
			delete f;
			delete node;
			node = next;
		}
	}
	ZeroMemory(fontMap, sizeof(fontMap));

	return TRUE;
}

FOXWRITING_API DOUBLE FWSetHAlign(DOUBLE align){
	halign = align;
	return TRUE;
}

FOXWRITING_API DOUBLE FWSetVAlign(DOUBLE align){
	valign = align;
	return TRUE;
}

FOXWRITING_API DOUBLE FWDrawText(DOUBLE x, DOUBLE y, CONST CHAR* str)
{
	if (currentFont == NULL){
		return FALSE;
	}

	int color = gm::draw_get_color();
	double alpha = gm::draw_get_alpha();
	return DrawTextInner(x, y, str, 0, 1, 1, 0, alpha, color, color);
}

FOXWRITING_API DOUBLE FWDrawTextEx(DOUBLE x, DOUBLE y, CONST CHAR* str, CONST CHAR* args){
	if (currentFont == NULL){
		return FALSE;
	}

	DOUBLE argsD[1];
	if (ParseArgs(args, argsD) < 1){
		return FALSE;
	}

	DOUBLE w = argsD[0];

	int color = gm::draw_get_color();
	double alpha = gm::draw_get_alpha();
	return DrawTextInner(x, y, str, w, 1, 1, 0, alpha, color, color);
}

FOXWRITING_API DOUBLE FWDrawTextTransformed(DOUBLE x, DOUBLE y, CONST CHAR* str, CONST CHAR* args)
{
	if (currentFont == NULL){
		return FALSE;
	}

	DOUBLE argsD[3];
	if (ParseArgs(args, argsD) < 3){
		return FALSE;
	}

	DOUBLE xscale = argsD[0];
	DOUBLE yscale = argsD[1];
	DOUBLE angle = argsD[2];

	int color = gm::draw_get_color();
	double alpha = gm::draw_get_alpha();
	return DrawTextInner(x, y, str, 0, xscale, yscale, angle, alpha, color, color);
}

FOXWRITING_API DOUBLE FWDrawTextTransformedEx(DOUBLE x, DOUBLE y, CONST CHAR* str, CONST CHAR* args)
{
	if (currentFont == NULL){
		return FALSE;
	}

	DOUBLE argsD[4];
	if (ParseArgs(args, argsD) < 4){
		return FALSE;
	}

	DOUBLE w = argsD[0];
	DOUBLE xscale = argsD[1];
	DOUBLE yscale = argsD[2];
	DOUBLE angle = argsD[3];

	int color = gm::draw_get_color();
	double alpha = gm::draw_get_alpha();
	return DrawTextInner(x, y, str, w, xscale, yscale, angle, alpha, color, color);
}

FOXWRITING_API DOUBLE FWDrawTextColor(DOUBLE x, DOUBLE y, CONST CHAR* str, CONST CHAR* args)
{
	if (currentFont == NULL){
		return FALSE;
	}

	DOUBLE argsD[3];
	if (ParseArgs(args, argsD) < 3){
		return FALSE;
	}

	DOUBLE color1 = argsD[0];
	DOUBLE color2 = argsD[1];
	DOUBLE alpha = argsD[2];

	return DrawTextInner(x, y, str, 0, 1, 1, 0, alpha, color1, color2);
}

FOXWRITING_API DOUBLE FWDrawTextColorEx(DOUBLE x, DOUBLE y, CONST CHAR* str, CONST CHAR* args)
{
	if (currentFont == NULL){
		return FALSE;
	}

	DOUBLE argsD[4];
	if (ParseArgs(args, argsD) < 4){
		return FALSE;
	}

	DOUBLE w = argsD[0];
	DOUBLE color1 = argsD[1];
	DOUBLE color2 = argsD[2];
	DOUBLE alpha = argsD[3];

	return DrawTextInner(x, y, str, w, 1, 1, 0, alpha, color1, color2);
}

FOXWRITING_API DOUBLE FWDrawTextTransformedColor(DOUBLE x, DOUBLE y, CONST CHAR* str, CONST CHAR* args)
{
	if (currentFont == NULL){
		return FALSE;
	}

	DOUBLE argsD[6];
	if (ParseArgs(args, argsD) < 6){
		return FALSE;
	}

	DOUBLE xscale = argsD[0];
	DOUBLE yscale = argsD[1];
	DOUBLE angle = argsD[2];
	DOUBLE color1 = argsD[3];
	DOUBLE color2 = argsD[4];
	DOUBLE alpha = argsD[5];

	return DrawTextInner(x, y, str, 0, xscale, yscale, angle, alpha, color1, color2);
}

FOXWRITING_API DOUBLE FWDrawTextTransformedColorEx(DOUBLE x, DOUBLE y, CONST CHAR* str, CONST CHAR* args)
{
	if (currentFont == NULL){
		return FALSE;
	}

	DOUBLE argsD[7];
	if (ParseArgs(args, argsD) < 7){
		return FALSE;
	}

	DOUBLE w = argsD[0];
	DOUBLE xscale = argsD[1];
	DOUBLE yscale = argsD[2];
	DOUBLE angle = argsD[3];
	DOUBLE color1 = argsD[4];
	DOUBLE color2 = argsD[5];
	DOUBLE alpha = argsD[6];

	return DrawTextInner(x, y, str, w, xscale, yscale, angle, alpha, color1, color2);
}

FOXWRITING_API DOUBLE FWAddFont(CONST CHAR* name, DOUBLE pt, DOUBLE style){
	if (pt <= 0){
		return -1;
	}

	USES_CONVERSION;
	LPCWSTR pName = A2W(name);

	FoxFont* font = new FoxFont();
	if (!font->SetFont((WCHAR*)pName, pt, (int)style)){
		delete font;
		return -1;
	}

	int index = fontCount;
	fontCount++;

	int mIndex = index % 256;

	PFontNode node = new FontNode;
	if (node == NULL){
		delete font;
		return -1;
	}

	node->index = index;
	node->font = font;
	node->next = fontMap[mIndex];
	fontMap[mIndex] = node;

	return index;
}

FOXWRITING_API DOUBLE FWAddFontFromFile(CONST CHAR* ttf, DOUBLE pt, DOUBLE style){
	if (pt <= 0){
		return -1;
	}

	USES_CONVERSION;
	LPCWSTR pName = A2W(ttf);

	FoxFont* font = new FoxFont();
	if (!font->SetFontFile((WCHAR*)pName, pt, (int)style)){
		delete font;
		return -1;
	}

	int index = fontCount;
	fontCount++;

	int mIndex = index % 256;

	PFontNode node = new FontNode;
	if (node == NULL){
		delete font;
		return -1;
	}

	node->index = index;
	node->font = font;
	node->next = fontMap[mIndex];
	fontMap[mIndex] = node;

	return index;
}

FOXWRITING_API DOUBLE FWSetFontOffset(DOUBLE font, DOUBLE xOffset, DOUBLE yOffset){
	if (font < 0){
		return FALSE;
	}

	int index = (int)font;
	int mIndex = index % 256;

	PFontNode node = fontMap[mIndex];
	while (node != NULL && node->index != index){
		node = node->next;
	}

	if (node == NULL){
		return FALSE;
	}

	node->font->SetOffset(xOffset, yOffset);

	return TRUE;
}

FOXWRITING_API DOUBLE FWSetFont(DOUBLE font){
	if (font < 0){
		return FALSE;
	}

	int index = (int)font;
	int mIndex = index % 256;

	PFontNode node = fontMap[mIndex];
	while (node != NULL && node->index != index){
		node = node->next;
	}

	if (node == NULL){
		return FALSE;
	}

	currentFont = node;

	return TRUE;
}

FOXWRITING_API DOUBLE FWDeleteFont(DOUBLE font){
	if (font < 0){
		return FALSE;
	}

	int index = (int)font;
	int mIndex = index % 256;

	for (PFontNode* curr = &fontMap[mIndex]; *curr;)
	{
		PFontNode entry = *curr;
		if (entry->index == index)
		{
			*curr = entry->next;

			if (currentFont == entry){
				currentFont = NULL;
			}
			delete entry->font;
			delete entry;
			return TRUE;
		}
		else {
			curr = &entry->next;
		}
	}

	return FALSE;
}

FOXWRITING_API DOUBLE FWEnablePixelAlignment(DOUBLE enable){
	int en = (int)enable;
	pixelAlign = en;

	return TRUE;
}

FOXWRITING_API DOUBLE FWStringWidth(CONST CHAR* str){
	if (currentFont == NULL){
		return 0;
	}
	USES_CONVERSION;
	LPCWSTR pStr = A2W(str);

	Gdiplus::SizeF size;
	MeasureString((WCHAR*)pStr, &size);

	return size.Width;
}

FOXWRITING_API DOUBLE FWStringHeight(CONST CHAR* str){
	if (currentFont == NULL){
		return 0;
	}
	USES_CONVERSION;
	LPCWSTR pStr = A2W(str);

	Gdiplus::SizeF size;
	MeasureString((WCHAR*)pStr, &size);

	return size.Height;
}

FOXWRITING_API DOUBLE FWStringWidthEx(CONST CHAR* str, DOUBLE sep, DOUBLE w){
	if (currentFont == NULL){
		return 0;
	}
	USES_CONVERSION;
	LPCWSTR pStr = A2W(str);

	Gdiplus::SizeF size;
	MeasureString((WCHAR*)pStr, &size, sep, w);

	return size.Width;
}

FOXWRITING_API DOUBLE FWStringHeightEx(CONST CHAR* str, DOUBLE sep, DOUBLE w){
	if (currentFont == NULL){
		return 0;
	}
	USES_CONVERSION;
	LPCWSTR pStr = A2W(str);

	Gdiplus::SizeF size;
	MeasureString((WCHAR*)pStr, &size, sep, w);

	return size.Height;
}

FOXWRITING_API DOUBLE FWSetLineSpacing(DOUBLE sep){
	lineSpacing = sep < 0 ? DEFAULT_SEP : sep;

	return TRUE;
}