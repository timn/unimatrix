
/***************************************************************************
 *  tnglue.h - Glue code to add some functions missing in 3.5 but present
 *             in 4.0+
 *
 *           - Color functions, glue to get 3.5 working...
 *             Or: Why the hell didn't they get WinSetForecolorRGB into 3.5!?
 *            - Pointer Retrieval
 *
 *
 *  Generated: 2002-07-11
 *  Copyright  2002-2005  Tim Niemueller [www.niemueller.de]
 *
 *  $Id: tnglue.c,v 1.6 2005/05/28 12:59:14 tim Exp $
 *
 ****************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <PalmOS.h>
#include "tnglue.h"

UInt8 gColorMode=COLORMODE_35;

Err
TNGlueColorInit(void)
{
  Err err;
  UInt32 romVersion;
  UInt32 requiredDepth = TN_GRAY_COLORDEPTH;
  UInt32 currentDepth = 0;
  Boolean hasColor=false;

  FtrGet(sysFtrCreator, sysFtrNumROMVersion, &romVersion);
  if (romVersion < 0x04000000) TNSetColorMode(COLORMODE_35);
  else TNSetColorMode(COLORMODE_40);

  // Get Display mode
  err = WinScreenMode(winScreenModeGet, NULL, NULL, &currentDepth, &hasColor);
  // Try to set higher, used to get inofficial gray scale support
  if (!hasColor && (currentDepth < requiredDepth)) {
    // Although we might set an err != errNone above we do not care about the result.
    // That may happen on some Palms that do not even support grayscale, like the
    // Palm III. This causes BadBugs(TM)
    err = WinScreenMode(winScreenModeSet, NULL, NULL, &requiredDepth, &hasColor);
  }
  return errNone;
}


Boolean
TNisColored(void)
{
  UInt32 currentDepth = 0;
  Boolean hasColor=false;
  WinScreenMode(winScreenModeGet, NULL, NULL, &currentDepth, &hasColor);
  return hasColor;
}


void
TNSetColorMode(UInt8 newmode)
{
  gColorMode=newmode;
}


static void
TNSetColorRGB(RGBColorType *new, RGBColorType *old, UInt8 type)
{
  if (gColorMode == COLORMODE_35) {
    IndexedColorType oldCI=0;
    IndexedColorType newCI=0;

    newCI=WinRGBToIndex(new);

    if (type == TN_CT_FORE)  oldCI=WinSetForeColor(newCI);
    else if (type == TN_CT_BACK)  oldCI=WinSetBackColor(newCI);
    else if (type == TN_CT_TEXT)  oldCI=WinSetTextColor(newCI);

    if (old != NULL)  WinIndexToRGB(oldCI, old);
  } else {

    if (type == TN_CT_FORE)  WinSetForeColorRGB(new, old);
    else if (type == TN_CT_BACK)  WinSetBackColorRGB(new, old);
    else if (type == TN_CT_TEXT)  WinSetTextColorRGB(new, old);

  }
}

void
TNSetForeColorRGB(RGBColorType *new, RGBColorType *old)
{
  TNSetColorRGB(new, old, TN_CT_FORE);
}


void
TNSetBackColorRGB(RGBColorType *new, RGBColorType *old)
{
  TNSetColorRGB(new, old, TN_CT_BACK);
}

void
TNSetTextColorRGB(RGBColorType *new, RGBColorType *old)
{
  TNSetColorRGB(new, old, TN_CT_TEXT);
}


/*****************************************************************************
* Function: TNDrawCharsToFitWidth
*
* Description:  Helper function to draw string in global find
*****************************************************************************/
void
TNDrawCharsToFitWidth(const char *s, RectanglePtr r)
{
  Int16   stringLength = StrLen(s);
  Int16   pixelWidth = r->extent.x;
  Boolean truncate;

  // FntCharsInWidth will update stringLength to the 
  // maximum without exceeding the width
  FntCharsInWidth(s, &pixelWidth, &stringLength, &truncate);
  WinDrawChars(s, stringLength, r->topLeft.x, r->topLeft.y);
}




UInt16
TNGetObjectIndexFromPtr(FormType *form, void *formObj)
{
  UInt16 index;
  UInt16 objIndex = frmInvalidObjectId;
  UInt16 numObjects = FrmGetNumberOfObjects(form);
  for (index = 0; index < numObjects; index++) {
    if (FrmGetObjectPtr(form, index) == formObj) {
    // Found it
    objIndex = index;
    break;
    }
  }

  return objIndex;
}


UInt32
TNPalmOSVersion(void)
 {
  UInt32 romVersion=0;

  FtrGet(sysFtrCreator, sysFtrNumROMVersion, &romVersion);

  return romVersion;
}


MemHandle
TNDmQueryPrevInCategory(DmOpenRef db, UInt16 *index, UInt16 category)
{
  UInt8 attrMask, attrCmp;
  UInt16 tmpIndex = 0;
  UInt16 count=0;
  UInt16 attr=0;

  // Form the mask for comparing the attributes of each record
  if (category == dmAllCategories) {
    attrMask =  dmRecAttrDelete;
    attrCmp = 0;
  } else {
    attrMask =  dmRecAttrCategoryMask | dmRecAttrDelete;
    attrCmp = category;
  }


  count = DmNumRecords(db);
  tmpIndex = *index +1; // +1 cause we decrement at beginning of loop
  while ( (tmpIndex > 0) && (tmpIndex <= count) ) {
    tmpIndex -= 1;
    DmRecordInfo(db, tmpIndex, &attr, NULL, NULL);
    if ((attr & attrMask) == attrCmp) {
      MemHandle m;
      m = DmQueryRecord(db, tmpIndex);

      *index = tmpIndex;
      return m;
    }
  }

  return NULL;
}


// Function description template
/*****************************************************************************
* FUNCTION:     
*
* DESCRIPTION:  
*
* PARAMETERS:   
* RETURNS:      
*****************************************************************************/
