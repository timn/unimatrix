/* $Id: database.c,v 1.1 2003/02/06 21:27:23 tim Exp $
 *
 * Database handling, another central piece in UniMatrix
 */

#include "database.h"
#include "ctype.h"

DmOpenRef gDatabase[DATABASE_NUM]={NULL, NULL};
UInt16    gCategory=0;

/*****************************************************************************
* Function:  OpenDatabase
*
* Description:  Opens/creates the application's database.
*****************************************************************************/
Err OpenDatabase(void) {
	Err err = errNone;
  UInt16 version;

  if (! gDatabase[DB_MAIN]) {
    gDatabase[DB_MAIN] = DmOpenDatabaseByTypeCreator(DATABASE_TYPE, APP_CREATOR, dmModeReadWrite);
    if (!gDatabase[DB_MAIN])  {
      LocalID dbID;
      err = CreateDatabase(DATABASE_NAME, &dbID);
      if (err == errNone)  gDatabase[DB_MAIN] = DmOpenDatabaseByTypeCreator(DATABASE_TYPE, APP_CREATOR, dmModeReadWrite);

    }
  }

  if (! gDatabase[DB_DATA]) {
    gDatabase[DB_DATA] = DmOpenDatabaseByTypeCreator(DATABASE_DATA_TYPE, APP_CREATOR, dmModeReadWrite);
    if (!gDatabase[DB_DATA]) {
      err = DmCreateDatabase(DATABASE_CARD, DATABASE_DATA_NAME, APP_CREATOR, DATABASE_DATA_TYPE, false);
      if (err == errNone)  gDatabase[DB_DATA] = DmOpenDatabaseByTypeCreator(DATABASE_DATA_TYPE, APP_CREATOR, dmModeReadWrite);
      if (gDatabase[DB_DATA]) {
        UInt8 i;
        for (i=0; i < CTYPE_DEF_NUM; ++i) {
          CourseTypeDBRecord ct;
          MemHandle m;
          UInt16 newIndex=dmMaxRecordIndex;

          MemSet(&ct, sizeof(ct), 0);
          ct.type = TYPE_CTYP;
          ct.id = i;

          m = DmGetResource(strRsc, CTYPE_DEF_START+i);
          StrCopy(ct.name, MemHandleLock(m));
          MemHandleUnlock(m);

          m = DmGetResource(strRsc, CTYPE_DEF_SH_START+i);
          StrCopy(ct.shortName, MemHandleLock(m));
          MemHandleUnlock(m);

          m = DmNewRecord(gDatabase[DB_DATA], &newIndex, sizeof(CourseTypeDBRecord));
          DmWrite(MemHandleLock(m), 0, &ct, sizeof(CourseTypeDBRecord));
          MemHandleUnlock(m);
          DmReleaseRecord(gDatabase[DB_DATA], newIndex, false);
        }
      }
    }
  }

  if (gDatabase[DB_MAIN]) {
    UniAppInfoType *appInfo;

    /* DEBUG code, needed after editing pdb in hex editor
    LocalID id;
    UInt32 time=TimGetSeconds();
    DmOpenDatabaseInfo(gDatabase[DB_MAIN], &id, NULL, NULL, NULL, NULL);
    DmSetDatabaseInfo(0, id, NULL, NULL, NULL, &time, &time, NULL, NULL, NULL, NULL, NULL, NULL);
    */
    appInfo = (UniAppInfoType *)MemLocalIDToLockedPtr(DmGetAppInfoID(gDatabase[DB_MAIN]), DATABASE_CARD);
    version = appInfo->version;
    MemPtrUnlock(appInfo);
    if (version < DATABASE_VERSION) {
      if (FrmAlert(ALERT_oldDB) == 0)  err = DatabaseConvert(version);
      else  err = dmErrCorruptDatabase;
    }
    // We will keep that for 0.7 until everybody converted, remove befor 1.0
  }

  return err;
} // OpenDatabase




/*****************************************************************************
* Function:  CloseDatabase
*
* Description:  Closes the application's database.
*****************************************************************************/
void CloseDatabase(void) {
	if (gDatabase[DB_MAIN])  DmCloseDatabase(gDatabase[DB_MAIN]);
  gDatabase[DB_MAIN] = NULL;
  if (gDatabase[DB_DATA])  DmCloseDatabase(gDatabase[DB_DATA]);
  gDatabase[DB_DATA] = NULL;
	// CleanupDatabase();
}

/*****************************************************************************
* Function:  CleanupDatabase
*
* Description:  Cleans up module global vars of database like gNumInfoRecord
*****************************************************************************/
void CleanupDatabase(void) {
  // Yet nothin' to do
}


/*****************************************************************************
* Function:  DatabaseGetRef
*
* Description: Returns the database pointer
*****************************************************************************/
DmOpenRef DatabaseGetRef(void) {
  return gDatabase[DB_MAIN];
}

/*****************************************************************************
* Function:  DatabaseGetRefN
*
* Description: Returns the database pointer from array
*****************************************************************************/
DmOpenRef DatabaseGetRefN(UInt8 num) {
  // We do not check to improbe performance, since this is NEVER a user give
  // value this just means that the programmer has to be careful, that should
  // not be a problem with nicely used constants
  // if (num >= DATABASE_NUM) return gDatabase[DB_MAIN];
  return gDatabase[num];
}



/*****************************************************************************
* Function:  DatabaseIsOpen
*
* Description: Returns true if database is open
*****************************************************************************/
Boolean DatabaseIsOpen(void) {
  if (gDatabase[DB_MAIN])  return true;
  else            return false;
}


/*****************************************************************************
* Function:  DeleteDatabase
*
* Description: Deletes the database
*****************************************************************************/
void DeleteDatabase(void) {
  LocalID dbID;

  dbID = DmFindDatabase(DATABASE_CARD, DATABASE_NAME);

  if (dbID != 0) {
    // Database does exist, we do not care about result.
    DmDeleteDatabase(DATABASE_CARD, dbID);
  }  

}

/*****************************************************************************
* Function: DatabaseConvert
*
* Description: Converts databases from older version to current version
*****************************************************************************/
Err DatabaseConvert(UInt16 oldVersion) {
  DmOpenRef old, new;
  LocalID newID, oldID;
  UInt16 numRecords, i, atP, DBversion=DATABASE_VERSION, attr, cat;
  UInt8 ctype;
  MemHandle oh, nh;
  Char *s, newName[32]=DATABASE_NAME;
  MemPtr p;
  UniAppInfoType *appInfoOld, *appInfoNew;

  if (! gDatabase[DB_MAIN])  return dmErrNoOpenDatabase;

  old = gDatabase[DB_MAIN];
  DmOpenDatabaseInfo(old, &oldID, NULL, NULL, NULL, NULL);
  appInfoOld = (UniAppInfoType *)MemLocalIDToLockedPtr(DmGetAppInfoID(old), DATABASE_CARD);

  CreateDatabase(DATABASE_TEMPNAME, &newID);
  new = DmOpenDatabase(DATABASE_CARD, newID, dmModeReadWrite);
  appInfoNew = (UniAppInfoType *)MemLocalIDToLockedPtr(DmGetAppInfoID(new), DATABASE_CARD);

  DmWrite(appInfoNew, 0, appInfoOld, sizeof(UniAppInfoType));
  DmWrite(appInfoNew, offsetof(UniAppInfoType, version), &DBversion, sizeof(DBversion));
  MemPtrUnlock(appInfoOld);
  MemPtrUnlock(appInfoNew);

  switch (oldVersion) {
    case 0:
    case 1:
      // We have a version one database, type to be converted is TimeDBRecord_v1

      numRecords=DmNumRecords(old);
      for (i=0; i < numRecords; ++i) {
        atP=i;
        oh = DmQueryRecord(old, i);

        DmRecordInfo(old, i, &attr, NULL, NULL);
        cat = attr & dmRecAttrCategoryMask;

        s = (Char *)MemHandleLock(oh);
        if (s[0] == TYPE_COURSE) {
          // We have to change the course type
          nh = DmNewRecord(new, &atP, MemPtrSize(s));
          if (nh) {
            p = MemHandleLock(nh);
            DmWrite(p, 0, s, MemPtrSize(s));
            s = (Char *)p;
            ctype = s[offsetof(CourseDBRecord, ctype)];
            // We added a course type...
            if (ctype > 1)  ctype += 1;              
            DmWrite(p, offsetof(CourseDBRecord, ctype), &ctype, sizeof(ctype));
            MemHandleUnlock(nh);
          }
          DmReleaseRecord(new, atP, false);
        } else {
          // Convert...
          TimeDBRecord_v1 *ot;
          TimeDBRecord *nt;

          nh = DmNewRecord(new, &atP, sizeof(TimeDBRecord));
          if (nh) {
            ot = (TimeDBRecord_v1 *)s;
            p = (TimeDBRecord *)MemHandleLock(nh);
            nt = (TimeDBRecord *)MemPtrNew(sizeof(TimeDBRecord));
            MemSet(nt, sizeof(TimeDBRecord), 0);
            nt->type = ot->type;
            nt->day = ot->day;
            nt->course = ot->course;
            nt->begin.hours = (UInt8)(ot->begin / 4) + 8;
            nt->begin.minutes = (ot->begin % 4) * 15;
            nt->end.hours = (UInt8)(ot->end / 4) + 8;
            nt->end.minutes = (ot->end % 4) * 15;
            nt->color[0] = ot->color[0];
            nt->color[1] = ot->color[1];
            nt->color[2] = ot->color[2];
            StrNCopy(nt->room, ot->room, 8);
            DmWrite(p, 0, nt, sizeof(TimeDBRecord));

            MemPtrFree((MemPtr)nt);
            MemHandleUnlock(nh);
          }
          DmReleaseRecord(new, atP, false);
        }


        DmRecordInfo(new, atP, &attr, NULL, NULL);
        attr |= cat;
        DmSetRecordInfo(new, atP, &attr, NULL);

        MemHandleUnlock(oh);
      }      

      break;
    default:
      DmCloseDatabase(new);
      return dmErrCorruptDatabase;
      break;
  }

  DmCloseDatabase(old);
  DmInsertionSort(new, DatabaseCompare, 0);
  gDatabase[DB_MAIN]=new;

  DmDeleteDatabase(DATABASE_CARD, oldID);
  DmSetDatabaseInfo(DATABASE_CARD, newID, newName, NULL, NULL, NULL, NULL, 
                    NULL, NULL, NULL, NULL, NULL, NULL);

  return errNone;
}


/*****************************************************************************
* Function:  CreateDatabase
*
* Description: Create a clean database. Used when receiving a beam
*****************************************************************************/
Err CreateDatabase(const Char *dbname, LocalID *dbID) {
  Err err;

  err = DmCreateDatabase(DATABASE_CARD, dbname, APP_CREATOR, DATABASE_TYPE, false);
  if (!err) {
    DmOpenRef db;
    UInt16 DBversion=DATABASE_VERSION;
    MemHandle h;
    LocalID appInfoID;
    UniAppInfoType *appInfoP;

    *dbID = DmFindDatabase(DATABASE_CARD, dbname);
    db = DmOpenDatabase(DATABASE_CARD, *dbID, dmModeReadWrite);
    if (!db) return DmGetLastErr();

    // Allocate app info in storage heap.
    h = DmNewHandle(db, sizeof(UniAppInfoType));
    if (!h) return dmErrMemError;
  
    // Associate app info with database.
    appInfoID = MemHandleToLocalID(h);
    DmSetDatabaseInfo(DATABASE_CARD, *dbID, NULL, NULL, NULL, NULL, NULL,
                      NULL, NULL, &appInfoID, NULL, NULL, NULL);
  
    // Initialize app info block to 0.
    appInfoP = MemHandleLock(h);
    DmSet(appInfoP, 0, sizeof(UniAppInfoType), 0);

    // Initialize the categories.
    CategoryInitialize ((AppInfoPtr) appInfoP, APP_CATEGORIES);
    DmWrite(appInfoP, offsetof(UniAppInfoType, version), &DBversion, sizeof(DBversion));

    // Unlock the app info block.
    MemPtrUnlock(appInfoP);

		DmCloseDatabase(db);

  }
  return err;
}


/*****************************************************************************
* Function:  DatabaseGetCat
*
* Description: Returns the current Category
*****************************************************************************/
UInt16 DatabaseGetCat(void) {
  return gCategory;
}

/*****************************************************************************
* Function:  DatabaseSetCat
*
* Description: Sets the current Category
*****************************************************************************/
void DatabaseSetCat(UInt16 newcat) {
  gCategory=newcat;
}



/*****************************************************************************
* Function:  CountCourses
*
* Description: Counts the courses saved in the given current category.
* Assumptions: This functions assumes, that the Records are SORTED with the
*              courses first and then the times.
*****************************************************************************/
UInt16 CountCourses(void) {
  MemHandle m;
  UInt16 index=0,count=0;

  while ((m = DmQueryNextInCategory(gDatabase[DB_MAIN], &index, gCategory))) {
    Char *s = MemHandleLock(m);
    if (s[0] == TYPE_COURSE) count += 1;
    MemHandleUnlock(m);
    index += 1;
  }

  return count;
}


void UnpackCourse(CourseDBRecord *course, const MemPtr mp) {

  const PackedCourseDBRecord *packedCourse = (PackedCourseDBRecord *)mp;
  const Char *s = packedCourse->name;

  course->type=packedCourse->type;
  course->ctype=packedCourse->ctype;
  course->id=packedCourse->id;
  course->name=(Char *)s;

  s += StrLen(s) + 1;
  course->teacherName = (Char *)s;

  s += StrLen(s) + 1;
  course->teacherEmail = (Char *)s;

  s += StrLen(s) + 1;
  course->teacherPhone = (Char *)s;

  s += StrLen(s) + 1;
  course->website = (Char *)s;
}

void PackCourse(CourseDBRecord *course, MemHandle courseDBEntry) {
  UInt16 length=0, offset=0;
  Char *s;

  length = sizeof(course->type) + sizeof(course->id) + sizeof(course->ctype)
           + StrLen(course->name) + 1 // +1 for String terminator
           + StrLen(course->teacherName) + 1
           + StrLen(course->teacherEmail) + 1
           + StrLen(course->teacherPhone) + 1
           + StrLen(course->website) + 1;


  if (MemHandleResize(courseDBEntry, length) == errNone) {
    s = MemHandleLock(courseDBEntry);
    offset = 0;
    // DmSet(s, offset, length, 0);

    DmWrite(s, offset, &course->type, sizeof(course->type));
    offset += sizeof(course->type);
    DmWrite(s, offset, &course->ctype, sizeof(course->ctype));
    offset += sizeof(course->ctype);
    DmWrite(s, offset, &course->id, sizeof(course->id));
    offset += sizeof(course->id);
    DmStrCopy(s, offset, course->name);
    offset += StrLen(course->name) + 1;
    DmStrCopy(s, offset, course->teacherName);
    offset += StrLen(course->teacherName) + 1;
    DmStrCopy(s, offset, course->teacherEmail);
    offset += StrLen(course->teacherEmail) + 1;
    DmStrCopy(s, offset, course->teacherPhone);
    offset += StrLen(course->teacherPhone) + 1;
    DmStrCopy(s, offset, course->website);

    MemHandleUnlock(courseDBEntry);

  } else {
    FrmAlert(ALERT_nomem);
  }

}

/*****************************************************************************
* Function:  DatabaseGetNewCID
*
* Description: Get's a new ID for a course
* Assumptions: This functions assumes, that the Records are SORTED with the
*              courses first and then the times.
*****************************************************************************/
UInt16 DatabaseGetNewCID(DmOpenRef cats, UInt16 category) {
  Err err=errNone;
  MemHandle m;
  UInt16 index=0,lastid=0;

  err = DmSeekRecordInCategory(cats, &index, 0, dmSeekForward, category);
  if (err != errNone) return 0;

  while ((m = DmQueryNextInCategory(cats, &index, category))) {
    Char *s = MemHandleLock(m);
    if (s[0] == TYPE_COURSE) {
      CourseDBRecord c;
      UnpackCourse(&c, s);

      if (c.id > lastid) lastid = c.id;

    }
    index += 1;
    MemHandleUnlock(m);
  }

  // lastid contains the last existing ID here, so return lastid+1 to get the first
  // non-existing one
  return lastid+1;
}


/*****************************************************************************
* Function:  DatabaseSort
*
* Description: Sorts the database, does a InsertionSort, since we excpect
*              only a very few records to be changed, probably only one
*****************************************************************************/
void DatabaseSort(void) {
  DmInsertionSort(gDatabase[DB_MAIN], DatabaseCompare, 0);
}


/*****************************************************************************
* Function:  DatabaseSortByDBRef
*
* Description: Sorts the database, does a InsertionSort, since we excpect
*              only a very few records to be changed, probably only one
*              Same as DatabaseSort but this will take an argument with the
*              opened database reference. This is needed for example when
*              receiving a beam and program was not active.
*****************************************************************************/
void DatabaseSortByDBRef(DmOpenRef db) {
  DmInsertionSort(db, DatabaseCompare, 0);
}


/*****************************************************************************
* Function:  DatabaseCompare
*
* Description: Compares two records
*****************************************************************************/
Int16 DatabaseCompare(void *rec1, void *rec2, Int16 other,
                      SortRecordInfoPtr rec1SortInfo, SortRecordInfoPtr rec2SortInfo, MemHandle appInfoH) {
  Char *s1, *s2;

  s1 = (Char *)rec1;
  s2 = (Char *)rec2;

  /* record types will be sorted descending: exams -> times -> courses
   * Same types will be sorted ascending
   */

  if (s1[0] > s2[0])  return -1;
  else if (s1[0]< s2[0]) return 1;
  else { //s1[0] == s2[0]
    if (s1[0] == TYPE_COURSE) {
      CourseDBRecord c1, c2;
      UnpackCourse(&c1, rec1);
      UnpackCourse(&c2, rec2);
      return StrCompare(c1.name, c2.name);
    } else if (s1[0] == TYPE_EXAM) {
      ExamDBRecord *e1=(ExamDBRecord *)rec1, *e2=(ExamDBRecord *)rec2;
      UInt32 d1, d2;
      d1=DateToDays(e1->date);
      d2=DateToDays(e2->date);
      if (d1 < d2)  return -1;
      else if (d1 > d2)  return 1;
      return 0;
    } else if (s1[0] == TYPE_TIME) {
      TimeDBRecord *t1, *t2;
      t1 = (TimeDBRecord *)rec1;
      t2 = (TimeDBRecord *)rec2;
      if (t1->day < t2->day)  return -1;
      else if (t1->day > t2->day)  return 1;
      else { // t1->day == t2->day
        UInt16 begin1, begin2;
        begin1 = TimeToInt(t1->begin);
        begin2 = TimeToInt(t2->begin);
        if (begin1 < begin2)  return -1;
        else if (begin1 > begin2)  return 1;
        return 0;
      }
    } else {
      // We don't know how to sort unknown types of data...
      return 0;
    }
  }
}
