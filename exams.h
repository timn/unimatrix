/* $Id: exams.h,v 1.3 2003/03/13 14:56:47 tim Exp $
 *
 * Everything you wanna know about exam code :-)
 * Created: 2002-07-11
 */



#define EXCOL_DONE 0
#define EXCOL_COURSE 1
#define EXCOL_DATE 2
#define EXCOL_TIME 3
#define EXCOL_SELI 4

#define EX_MAX_ROWS 10
#define EX_NUM_COLS 4
#define EX_NUM_HANDLECOLS 3

// That is just for maso-coders...
#define MAC_SHIT_YEAR_CONSTANT 1904

#define EX_FLAG_DONE 0x0001

extern Boolean ExamsFormHandleEvent(EventPtr event) SECOND_SECTION;
extern Boolean ExamDetailsFormHandleEvent(EventPtr event) SECOND_SECTION;