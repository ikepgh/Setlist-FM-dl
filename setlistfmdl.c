/* Setlist-FM-dl 0.7 by IKE (compiled with gcc 2.95.3/MorphOS)
 *
 * Connects to Setlist.fm user and downloads various data
 *
 * ike@ezcyberspace.com   Date: 2/24/20
 *
 * - NList/parsecsv code based on ViewCSV. Thanks Watertonian!
 *
 * - .xml-to-.html code based on libxslt_tutorial.c and xsltproc.c
 *   by John Fleck and Daniel Veillard respectively  ...Thanks!
 *
 * - Download entire user concert history in .csv format uses Rob Medico's
 *   frontend at https://backup-setlistfm.herokuapp.com  ...Thanks!
 *
 * gcc -o Setlist-FM-dl setlistfmdl.c -lcurl -lssl -lcrypto -ldl -lpthread -lxml2 -lxslt -lz -liconv -lm -s -Wall
 */
#include <stdio.h>
#include <stdlib.h>                       
#include <string.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <clib/gadtools_protos.h>
#include <MUI/BetterString_mcc.h>
#include <MUI/NListview_mcc.h>
#include <MUI/Hyperlink_mcc.h>
#include "GG:includestd/curl/curl.h"
#include <SDI/SDI_hook.h>
#include <libxml/xmlmemory.h>
#include <libxml/debugXML.h>
#include <libxml/HTMLtree.h>
#include <libxml/xmlIO.h>
#include <libxml/DOCBparser.h>
#include <libxml/xinclude.h>
#include <libxml/catalog.h>
#include <libxslt/xslt.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

char *version = "$VER: Setlist-FM-dl-0.7";

struct GfxBase *GfxBase;
struct IntuitionBase *IntuitionBase;
struct Library *MUIMasterBase;
struct Library *SocketBase;

extern int xmlLoadExtDtdDefaultValue;

#define MAKE_ID(a,b,c,d) ((ULONG) (a)<<24 | (ULONG) (b)<<16 | (ULONG) (c)<<8 | (ULONG) (d))

/// Button #defines
// Main Buttons
#define GRABSEARCH    45
#define GRAB          46
#define GRABCLEAR     47
// Settings Buttons
#define SAVE          48
#define USE           49
#define CANCEL        50
// Artists Buttons
#define ARTISTS       51
#define YES_ARTISTS   52
// Cities Buttons
#define CITIES        53
#define YES_CITIES    54
// Countries Buttons
#define BUT_COUNTRIES 55
#define YES_COUNTRIES 56
// Setlists Buttons
#define SETLISTS      57
#define YES_SETLISTS  58
// Venues Buttons
#define VENUES        59
#define YES_VENUES    60
// UserID Buttons
#define USERID        61
#define YES_USERID    62
// UserID Attended Buttons
#define USERIDATTENDED     63
#define YES_USERIDATTENDED 64
// UserID Edited Buttons
#define USEREDITED     65
#define YES_USEREDITED 66
#define CLEAR_USERID   67
///

enum {
	ADD_METHOD = 1,

	MEN_PROJECT, MEN_ABOUT, MEN_ABOUTMUI, MEN_QUIT, MEN_SEARCH, MEN_SETLISTFM,
    MEN_SETTINGS, MEN_PREFS, MEN_DELETE, MEN_MUIPREFS,
};

static struct NewMenu MenuData1[]=
{
	{NM_TITLE, "Project",                      0 , 0, 0,  (APTR)MEN_PROJECT      },
	{NM_ITEM,  "About...",                    "?", 0, 0,  (APTR)MEN_ABOUT        },
	{NM_ITEM,  "About MUI...",				   0,  0, 0,  (APTR)MEN_ABOUTMUI     },
	{NM_ITEM,  NM_BARLABEL,                    0 , 0, 0,  (APTR)0                },
	{NM_ITEM,  "Quit",                        "Q", 0, 0,  (APTR)MEN_QUIT         },
    {NM_TITLE, "Search",         	    	   0 , 0, 0,  (APTR)MEN_SEARCH       },
    {NM_ITEM,  "Search Setlist.fm",           "S", 0, 0,  (APTR)MEN_SETLISTFM    },
    {NM_TITLE, "Settings",       	    	   0 , 0, 0,  (APTR)MEN_SETTINGS     },
    {NM_ITEM,  "Preferences...",               0 , 0, 0,  (APTR)MEN_PREFS        },
    {NM_ITEM,  "Delete 'output.csv'",    	   0 , 0, 0,  (APTR)MEN_DELETE       },
	{NM_ITEM,  NM_BARLABEL,                    0 , 0, 0,  (APTR)0                },
    {NM_ITEM,  "MUI...",	                   0 , 0, 0,  (APTR)MEN_MUIPREFS     },
    {NM_END,   NULL,                           0 , 0, 0,  (APTR)0                },
};

char about_text[] =
"\33cSetlist-FM-dl © 2020 IKE\n version 0.7 (2/24/20)\n Connects to Setlist.fm and downloads various data\n";

Object *STR_tag, *STR_langsetting, *STR_xapikeysetting, *STR_userid, *aboutwin, *STR_userid_attended,
*STR_userid_attended_page, *STR_artists, *STR_artists_page, *STR_venues, *STR_venues_page, *STR_venues_html,
*STR_cities, *STR_cities_page, *STR_cities_html, *STR_artists_html, *STR_countries_html, *STR_setlists,
*STR_setlists_page, *STR_setlists_html, *STR_userid_edited, *STR_userid_edited_page, *STR_userid_html,
*STR_userid_attended_html, *STR_userid_edited_html, *STR_setlists_date, *STR_page_number, *STR_setlists_tourname,
*check_artists_convert, *check_artists, *check_setlists_convert, *check_cities_convert, *check_venues_convert,
*check_userid_attended_convert, *check_userid_edited_convert, *check_userid_convert, *check_countries_convert;

APTR app, grab_gad, grab_search, clear_tag, but_save, but_use, but_cancel, win_settings, but_countries,
but_countries_yes,  but_userid_search, but_userid_clear, but_userid_yes, but_userid_attended_search,
but_userid_attended_yes, win_setlistfm, but_artists_search, but_artists_yes, but_venues_search, but_venues_yes,
but_cities_search, but_cities_yes, but_setlists_search, but_setlists_yes, but_userid_edited_search, but_userid_edited_yes,
artists_html_result, artists_setlistfm_html, cities_html_result, cities_setlistfm_html, countries_html_result,
countries_setlistfm_html, venues_html_result, venues_setlistfm_html, setlists_html_result, setlists_setlistfm_html,
userid_html_result, userid_setlistfm_html, userid_attended_html_result, userid_attended_setlistfm_html,
userid_edited_html_result, userid_edited_setlistfm_html, main_setlistfm_html, apikey_setlistfm_html;

BOOL running = TRUE;

// cURL
CURL *curl;
CURLcode code;
long res;

char *tag, *langcode, *xapikey, *userid, *userid_attended, *artists, *venues, *cities, *setlists, *setlists_date,
*setlists_tourname, *userid_edited, *xapikey_var, *langcode_var, *setlistapi_html, *setlistapi_xml;
char host[1024];
char filename[] = "ram:output.csv";
char setlistfm_filename[] = "ram:setlistfm.csv";

// PageNumber
char str[5000];
#define IPTR ULONG

/// Open_Libs
BOOL Open_Libs(BOOL active) {

	if(active) {

		if ( !(IntuitionBase=(struct IntuitionBase *) OpenLibrary("intuition.library",39)) ) return(0);
		if ( !(GfxBase=(struct GfxBase *) OpenLibrary("graphics.library",0)) ) {

			CloseLibrary((struct Library *)IntuitionBase);
			return(0);
		};
		if ( !(MUIMasterBase=OpenLibrary(MUIMASTER_NAME,19)) ) {

			CloseLibrary((struct Library *)GfxBase);
			CloseLibrary((struct Library *)IntuitionBase);
		    return(0);
		};
	}
	else {

		if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
		if (GfxBase) CloseLibrary((struct Library *)GfxBase);
		if (MUIMasterBase) CloseLibrary(MUIMasterBase);
		return(0);
	};
	return(1);
};
///

/// get_PageNumber
void get_PageNumber(void)  
{
    IPTR slideValue = 0;
    get(STR_page_number, MUIA_Numeric_Value, &slideValue); 
    sprintf(str, "%ld", (long)slideValue);    
    }
///

/// csv declarations
enum {list_csv, list_info, list_lastentry};
enum {group_main, group_toolbar, group_mainview, group_lastentry};
enum {window_main, window_lastentry};
enum {mui_notify, mui_list};
APTR lists[list_lastentry], listviews[list_lastentry], application(), titleslider, groups[group_lastentry], windows[window_lastentry];
char maintitle[255];
char *wintitles[] = {"Setlist-FM-dl - No file loaded", "File information", "Entry information", NULL};
enum {string_loaded, string_cols, string_rows, string_blankentry, string_titlebar, string_lastentry};
char *strings[] = {"Loaded", "Cols:", "Rows:", "", "BAR,", NULL};
char blank_entry[255];
enum{sepcode_comma, sepcode_tab, sepcode_semicolon, sepcode_cr, sepcode_quote, sepcode_custom, sepcode_lastentry};
char *sepcodes = ",\t;\n\"";
char *master_separator[]={",", "TAB", ";", "CR", "\" ","Custom", NULL};
char *labels_separator[sepcode_lastentry], *labels_escape[sepcode_lastentry], *labels_newline[sepcode_lastentry], *currentfile;
struct CSVfile
{
	char separators[3];
	int titlerow;
	int columns;
	int rows;
	int titlecolumn;
	int filesize;
	LONG *offsets;
	char *data;
	LONG blanktag;
};
LONG *csvoffsets;
int *rowlist, *infolist;
char *colinfolist[255];
char csvtitle[1024];
enum{csv_newfield, csv_escape, csv_newrow};
struct CSVfile myCSV=
{
	",\"\n",
	0,
	0,0,0,
	0,
	0,
	"No File Loaded\n",
};
int csvsort_col = 0;
///

/// Hook declarations
HOOKPROTONH(MuiGadFunc, LONG, Object *app, LONG *args); MakeStaticHook(MuiGadHook, MuiGadFunc);
HOOKPROTONH(nlistFunc, long, char **array, LONG *args); MakeStaticHook(nlistHook, nlistFunc);
HOOKPROTONH(compareFunc, LONG, LONG cst1, LONG cst2); MakeStaticHook(compareHook, compareFunc);
///

/// quickload function - Automagically allocate memory, load a file to it, and return memory pointer
#define quickfile_gauge(a) 
#define quickfile_set(a) 
#define quickfile_clear(a) 

char *quickfile_errors[] = {"No Error", "Memory Error", "File not found", "Save file error", "Wrong file type", NULL};
enum {quickfile_noerror, quickfile_memoryerror, quickfile_notfounderror, quickfile_saveerror, quickfile_wrongfile, quickfile_lastentry};
int quickfile_ecode = 0;
int quickfile_filesize = 0;

char *quickload(char *filebuffer, char *filename, char *header) {

	int ecode = 0;
	int filesize = 0;
	int headersize = 0;
	int count = 0;
	int headeroffset = 0;
	char tch = 0;
	FILE *inputfile;
	quickfile_ecode = quickfile_noerror;
	if((inputfile = fopen(filename, "r"))) {
		fseek(inputfile, 0, SEEK_END);
		filesize = (ftell(inputfile)+1)*sizeof(char);
		filebuffer = NULL;
		filebuffer = (char *)realloc((char *)filebuffer, filesize);
		quickfile_filesize = filesize;
		if(filebuffer == NULL) {
			filebuffer = (char *)realloc((char *)filebuffer, (strlen(quickfile_errors[quickfile_memoryerror])+1));
			sprintf(filebuffer, "%s", quickfile_errors[quickfile_memoryerror]);
		}
		else {
			fseek(inputfile, 0, SEEK_SET);
			if(header == NULL) {
				headersize=-1;
			}
			else {
				headersize = strlen(header);
			};
			quickfile_set(filesize);
			for(count = 0; count < filesize; count++) {
				quickfile_gauge(count);
				if(count == headersize) {
					headeroffset = count;
					filebuffer[count] = 0;
					if(strcmp(filebuffer, header)!= 0) {
						count = filesize;
						filebuffer = NULL;
						filebuffer = (char *)realloc((char *)filebuffer, (strlen(quickfile_errors[quickfile_wrongfile])+1));
						sprintf(filebuffer, "%s", quickfile_errors[quickfile_wrongfile]);
						ecode = quickfile_wrongfile;
                        quickfile_ecode = quickfile_wrongfile;
					};
				};
				if(ecode == quickfile_noerror) {
					tch = fgetc(inputfile);
						filebuffer[count-headeroffset] = tch;
				};
			};
			filebuffer[count-1] = 0;
			if(ecode == quickfile_noerror) {
				if(header == NULL) headersize = 0;
				filebuffer[filesize-headersize] = 0;
			};
			quickfile_clear(0);
		};
	}
	else {
		filebuffer = (char *)realloc((char *)filebuffer, (strlen(quickfile_errors[quickfile_notfounderror]+1)));
		sprintf(filebuffer, "%s", quickfile_errors[quickfile_notfounderror]);
		ecode = quickfile_notfounderror;
        quickfile_ecode = quickfile_notfounderror;
	};
	fclose(inputfile);
	return(filebuffer);
};
///

/// dolist function
void dolist(int list_id, int list_entry) {
	LONG result;
    int count = 0;
	switch(list_id) {
		case list_csv:		  
			if(result) {
				DoMethod(lists[list_info], MUIM_Set,MUIA_Disabled, TRUE);
				DoMethod(lists[list_info], MUIM_NList_Clear);
				for(count = 0; count < myCSV.columns;count++) {
					infolist[count] = ((list_entry+1)*myCSV.columns)+count;
					DoMethod(lists[list_info], MUIM_NList_InsertSingle, infolist[count], MUIV_NList_Insert_Bottom);
				};
				DoMethod(lists[list_info] ,MUIM_Set, MUIA_Disabled, FALSE);
			};
		break;
	};
};
///

/// parsecsv function
void parsecsv(char *csvfile) {
	int count = 0; int countlen = 0; BOOL seedata = TRUE; int currentcols = 0; int currentrows = 0; int fieldcount = 0;
	char etext[255];

	currentfile = NULL;
	currentfile = quickload(currentfile, csvfile, NULL);
	if(quickfile_ecode == quickfile_noerror) {
		sprintf(maintitle, "%s - %s", "Setlist-FM-dl", csvfile);
		DoMethod(windows[window_main], MUIM_Set, MUIA_Window_Title, maintitle);
		DoMethod(listviews[list_csv], MUIM_Set, MUIA_Disabled, TRUE);
		countlen = strlen(currentfile);
		myCSV.filesize = countlen;
		myCSV.blanktag = countlen+1;
		myCSV.data = currentfile;
		myCSV.rows = 0;
		myCSV.columns = 0;
		quickfile_clear(0);
		quickfile_set(countlen);
		DoMethod(lists[list_csv], MUIM_NList_Clear);
		for(count = 0; count < countlen; count++) {
			quickfile_gauge(count);
			if(myCSV.data[count] == myCSV.separators[csv_newfield]) {
				if(seedata) {
					myCSV.data[count] = 0;
					currentcols++;
					if(currentcols > myCSV.columns) myCSV.columns = currentcols;
				};
			};
			if(myCSV.data[count] == myCSV.separators[csv_newrow]) {
				if(seedata) {
					myCSV.data[count] = 1;
					myCSV.rows++;
					currentcols++;
					if(currentcols > myCSV.columns) myCSV.columns = currentcols;
					currentcols = 0;
				};
			};
			if(myCSV.data[count] == myCSV.separators[csv_escape]) {
				if(seedata) seedata = FALSE;
				else seedata = TRUE;
			};
		};
		DoMethod(titleslider, MUIM_Set, MUIA_Slider_Max, myCSV.rows);
		sprintf(csvtitle, "");
		myCSV.titlerow = 1;
		for(count = 0; count < myCSV.columns; count++)sprintf(csvtitle, "%s%s", csvtitle, strings[string_titlebar]);
		csvoffsets = (LONG *)realloc((LONG *)csvoffsets, (sizeof(int)*(1+(myCSV.rows*myCSV.columns))));
		rowlist = (int *)realloc((int *)rowlist, (myCSV.rows*sizeof(int)));
		infolist = (int *)realloc((int *)infolist, (myCSV.columns*sizeof(int)));
		if((csvoffsets == NULL)||(rowlist == NULL)||(infolist == NULL)) {
			printf("MUI Memory error...\n");  
		}
		else {
			myCSV.offsets = csvoffsets;
			myCSV.offsets[0] = 0;
			fieldcount = 1;
			quickfile_clear(0);
			quickfile_set(countlen);
			currentcols = 0;
			currentrows = 0;
			for(count = 0; count < countlen; count++) {
				quickfile_gauge(count);
				if((unsigned char)myCSV.data[count] <= 1) {
					myCSV.offsets[fieldcount] = count+1;
					currentcols++;
					fieldcount++;
					if(myCSV.data[count] == 1) {
						while(currentcols < myCSV.columns) {
							myCSV.offsets[fieldcount] = count+1;
							myCSV.offsets[fieldcount-1] = myCSV.blanktag;
							fieldcount++; currentcols++;
						};
						myCSV.data[count] = 0;
						rowlist[currentrows] = currentrows;
						if(currentrows==0) DoMethod(lists[list_csv], MUIM_Set, MUIA_NList_Format, csvtitle);
						if((currentrows+1)!= myCSV.titlerow) DoMethod(lists[list_csv], MUIM_NList_InsertSingle, rowlist[currentrows]+1, MUIV_NList_Insert_Bottom);
						currentrows++;
						currentcols = 0;
					};
				};
			};
			quickfile_clear(0);
			sprintf(etext, "%s %s(%s%i %s%i)", csvfile, strings[string_loaded], strings[string_cols], myCSV.columns, strings[string_rows], myCSV.rows);
			DoMethod(listviews[list_csv], MUIM_Set, MUIA_Disabled, FALSE);
		};
	};
};
///

/****Setlist.fm Search Function******/

/// get_setlistapi function - connects to Setlist.fm and downloads data file to Ram:
int get_setlistapi() {
       
    struct curl_slist *list = NULL;

    list = curl_slist_append(list, "Accept:application/xml");
    list = curl_slist_append(list, xapikey_var);  
    list = curl_slist_append(list, langcode_var);
    list = curl_slist_append(list, "Accept: charset=utf-8"); 

	curl = curl_easy_init();

    if(!curl) {
        printf("Error connecting...\n");
        return -1;
    }

	curl_easy_setopt(curl, CURLOPT_URL, host);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, stdout);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);

	freopen(filename, "w", stdout);

	code = curl_easy_perform(curl);

    if(code) {
        printf("Error connecting...\n");
		return -1;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res);

    if(res != 200) {
 	}
	
    fclose(stdout);

	curl_easy_cleanup(curl);
    curl_slist_free_all(list);
    strcpy(xapikey, "");
    strcpy(langcode, "");
    parsecsv(filename);
	return (0);
};
///

/****Setlist.fm HTML Function********/

/// get_setlistapi_html function - parses .xml and creates .html file
int get_setlistapi_html(){
    xsltStylesheetPtr cur; // style sheet
	xmlDocPtr doc, res;  // input file & applying style sheet
    FILE *output_file;

    output_file = fopen(setlistapi_html, "w");

	xmlSubstituteEntitiesDefault(1);
	xmlLoadExtDtdDefaultValue = 1;
	cur = xsltParseStylesheetFile((const xmlChar *)setlistapi_xml);
	doc = xmlParseFile(filename);
	res = xsltApplyStylesheet(cur, doc, NULL);
    xsltSaveResultToFile(output_file, res, cur);

	xsltFreeStylesheet(cur);
	xmlFreeDoc(res);
	xmlFreeDoc(doc);
    fclose(output_file);
    xsltCleanupGlobals();
    xmlCleanupParser();
	return(0);
}
///

/****CheckMark Functions*************/

/// CheckMark
LONG xget(Object *obj,ULONG attribute)
{
	LONG x;
	get(obj,attribute,&x);
	return(x);
}

BOOL getbool(Object *obj)
{
	return((BOOL)xget(obj,MUIA_Selected));
}

Object *MakeCheck(BOOL check)
{
	Object *obj = MUI_MakeObject(MUIO_Checkmark,NULL);
	if (obj) set(obj,MUIA_CycleChain,1);
	return(obj);
}
///

// notify function
void notify(int groupname, BOOL active) {
	if(groupname == group_main) {
		if(active) {

/// DoMethods
			DoMethod(app, MUIM_Application_Load, MUIV_Application_Load_ENV);
			DoMethod(app, MUIM_Application_Load, MUIV_Application_Load_ENVARC);
///

			DoMethod(windows[window_main], MUIM_Notify, MUIA_Window_CloseRequest, TRUE, app, 5, MUIM_CallHook, &MuiGadHook, mui_notify, group_main, FALSE);
			DoMethod(windows[window_main], MUIM_Set, MUIA_Window_Open, TRUE);
			DoMethod(lists[list_csv], MUIM_Notify, MUIA_NList_Active, MUIV_EveryTime, app, 5, MUIM_CallHook, &MuiGadHook, mui_list, list_csv, MUIV_TriggerValue);

/// Prefs DoMethods
		    DoMethod(win_settings, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
			win_settings, 3, MUIM_Set, MUIA_Window_Open, FALSE);

			DoMethod(but_save, MUIM_Notify, MUIA_Pressed, FALSE,
			app, 2, MUIM_Application_ReturnID, SAVE);

			DoMethod(but_use, MUIM_Notify, MUIA_Pressed, FALSE,
			app, 2, MUIM_Application_ReturnID, USE);

			DoMethod(but_cancel, MUIM_Notify, MUIA_Pressed, FALSE,
			app, 2, MUIM_Application_ReturnID, CANCEL);

			DoMethod(win_settings,MUIM_Window_SetCycleChain, STR_xapikeysetting,
    		STR_langsetting, but_save, but_use, but_cancel, NULL);
///

/// Search Window DoMethod
            DoMethod(win_setlistfm, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
		    win_setlistfm, 3, MUIM_Set, MUIA_Window_Open, FALSE);
///

/// Setlists DoMethods
			DoMethod(but_setlists_search, MUIM_Notify, MUIA_Pressed, FALSE,
			app, 2, MUIM_Application_ReturnID, SETLISTS);

            DoMethod(but_setlists_yes, MUIM_Notify, MUIA_Pressed, FALSE,
			app, 2, MUIM_Application_ReturnID, YES_SETLISTS);

	        DoMethod(check_setlists_convert, MUIM_Notify, MUIA_Selected, MUIV_EveryTime,
            but_setlists_yes,3, MUIM_Set, MUIA_Disabled, MUIV_TriggerValue);
///

/// Artists DoMethods
			DoMethod(but_artists_search, MUIM_Notify, MUIA_Pressed, FALSE,
			app, 2, MUIM_Application_ReturnID, ARTISTS);

            DoMethod(but_artists_yes, MUIM_Notify, MUIA_Pressed, FALSE,
			app, 2, MUIM_Application_ReturnID, YES_ARTISTS);

	        DoMethod(check_artists_convert, MUIM_Notify, MUIA_Selected, MUIV_EveryTime,
            but_artists_yes,3, MUIM_Set, MUIA_Disabled, MUIV_TriggerValue);
///

/// Cities DoMethods
			DoMethod(but_cities_search, MUIM_Notify, MUIA_Pressed, FALSE,
			app, 2, MUIM_Application_ReturnID, CITIES);

            DoMethod(but_cities_yes, MUIM_Notify, MUIA_Pressed, FALSE,
			app, 2, MUIM_Application_ReturnID, YES_CITIES);

	        DoMethod(check_cities_convert, MUIM_Notify, MUIA_Selected, MUIV_EveryTime,
            but_cities_yes,3, MUIM_Set, MUIA_Disabled, MUIV_TriggerValue);
///

/// Venues DoMethods
			DoMethod(but_venues_search, MUIM_Notify, MUIA_Pressed, FALSE,
			app, 2, MUIM_Application_ReturnID, VENUES);

            DoMethod(but_venues_yes, MUIM_Notify, MUIA_Pressed, FALSE,
			app, 2, MUIM_Application_ReturnID, YES_VENUES);

	        DoMethod(check_venues_convert, MUIM_Notify, MUIA_Selected, MUIV_EveryTime,
            but_venues_yes,3, MUIM_Set, MUIA_Disabled, MUIV_TriggerValue);
///

/// UserID Attended DoMethods
			DoMethod(but_userid_attended_search, MUIM_Notify, MUIA_Pressed, FALSE,
			app, 2, MUIM_Application_ReturnID, USERIDATTENDED);

            DoMethod(but_userid_attended_yes, MUIM_Notify, MUIA_Pressed, FALSE,
			app, 2, MUIM_Application_ReturnID, YES_USERIDATTENDED);

	        DoMethod(check_userid_attended_convert, MUIM_Notify, MUIA_Selected, MUIV_EveryTime,
            but_userid_attended_yes,3, MUIM_Set, MUIA_Disabled, MUIV_TriggerValue);
///

/// UserID Edited DoMethods
			DoMethod(but_userid_edited_search, MUIM_Notify, MUIA_Pressed, FALSE,
			app, 2, MUIM_Application_ReturnID, USEREDITED);

            DoMethod(but_userid_edited_yes, MUIM_Notify, MUIA_Pressed, FALSE,
			app, 2, MUIM_Application_ReturnID, YES_USEREDITED);

	        DoMethod(check_userid_edited_convert, MUIM_Notify, MUIA_Selected, MUIV_EveryTime,
            but_userid_edited_yes,3, MUIM_Set, MUIA_Disabled, MUIV_TriggerValue);
///

/// UserID DoMethods
			DoMethod(but_userid_search, MUIM_Notify, MUIA_Pressed, FALSE,
			app, 2, MUIM_Application_ReturnID, USERID);

            DoMethod(but_userid_clear, MUIM_Notify, MUIA_Pressed, FALSE,
			app, 2, MUIM_Application_ReturnID, CLEAR_USERID);

            DoMethod(but_userid_yes, MUIM_Notify, MUIA_Pressed, FALSE,
			app, 2, MUIM_Application_ReturnID, YES_USERID);

	        DoMethod(check_userid_convert, MUIM_Notify, MUIA_Selected, MUIV_EveryTime,
            but_userid_yes,3, MUIM_Set, MUIA_Disabled, MUIV_TriggerValue);
///

/// Countries DoMethods
			DoMethod(but_countries, MUIM_Notify, MUIA_Pressed, FALSE,
			app, 2, MUIM_Application_ReturnID, BUT_COUNTRIES);

			DoMethod(but_countries_yes, MUIM_Notify, MUIA_Pressed, FALSE,
			app, 2, MUIM_Application_ReturnID, YES_COUNTRIES);

	        DoMethod(check_countries_convert, MUIM_Notify, MUIA_Selected, MUIV_EveryTime,
            but_countries_yes,3, MUIM_Set, MUIA_Disabled, MUIV_TriggerValue);
///

/// Main Window DoMethods
			DoMethod(clear_tag, MUIM_Notify, MUIA_Pressed, FALSE,
			app, 2, MUIM_Application_ReturnID, GRABCLEAR);

			DoMethod(grab_gad, MUIM_Notify, MUIA_Pressed, FALSE,
			app, 2, MUIM_Application_ReturnID, GRAB);

			DoMethod(grab_search, MUIM_Notify, MUIA_Pressed, FALSE,
			app, 2, MUIM_Application_ReturnID, GRABSEARCH);
///

/// get STR_*
            get(STR_tag, MUIA_String_Contents, &tag);
			get(STR_langsetting, MUIA_String_Contents, &langcode);
		    get(STR_xapikeysetting, MUIA_String_Contents, &xapikey);
            get(STR_userid, MUIA_String_Contents, &userid);
            get(STR_userid_attended, MUIA_String_Contents, &userid_attended);
            get(STR_artists, MUIA_String_Contents, &artists);
            get(STR_venues, MUIA_String_Contents, &venues);
            get(STR_cities, MUIA_String_Contents, &cities);
            get(STR_setlists, MUIA_String_Contents, &setlists);
            get(STR_userid_edited, MUIA_String_Contents, &userid_edited);
///
			}
			else {
				DoMethod(windows[window_main], MUIM_KillNotify, MUIA_Window_CloseRequest);
				DoMethod(lists[list_csv], MUIM_KillNotify, MUIA_NList_Active);
				DoMethod(windows[window_main], MUIM_Set, MUIA_Window_Open, FALSE);
				running = FALSE;
			};
	};
};

/// csv Hook
HOOKPROTONH(MuiGadFunc, LONG, Object *app, LONG *args) {
	int ecode = 0;
	switch (args[0]) {
		case mui_list:
			dolist(args[1], args[2]);
		break;
		case mui_notify:
		    notify(args[1], args[2]);
		break;
	}
	return(ecode);
};

HOOKPROTONH(compareFunc, LONG, LONG cst1, LONG cst2) {
	char string1[255]; char string2[255]; int ecode = 0;
	sprintf(string1, "%s", myCSV.data+myCSV.offsets[(cst1*myCSV.columns)+csvsort_col]);
	sprintf(string2, "%s", myCSV.data+myCSV.offsets[(cst2*myCSV.columns)+csvsort_col]);
	ecode = stricmp((char *)string2, (char *)string1);
	return(ecode);
};

HOOKPROTONH(nlistFunc, long, char **array, LONG *args) {
	int ecode = 0;
	int count = 0; int rowstart = 0;
	if(strcmp(array[0], "Setlist-FM-dl") == 0) {
		if(myCSV.titlerow > 0) {
			rowstart = ((myCSV.titlerow)-1)*myCSV.columns;
			for(count = 0; count < myCSV.columns; count++) {
				array[count] = (char *)myCSV.data+myCSV.offsets[rowstart+count];
			};
		}
		else {
			for(count = 0; count < myCSV.columns; count++) {
				array[count] = "Setlist-FM-dl";
			};
		};
	}
	else {
		rowstart = (((int)array[0])-1)*myCSV.columns;
		for(count = 0; count < myCSV.columns; count++) {
			if(myCSV.offsets[count+rowstart] > myCSV.filesize) array[count] = blank_entry;
			else array[count] = (char *)myCSV.data+myCSV.offsets[rowstart+count];
		};
	};
	return(ecode);
};

// initdata
BOOL initdata(int argC, char *argV[]) {
	int mostelements = 0; int count = 0; int ecode = 0;
	struct WBStartup *argmsg;
	if(sepcode_lastentry > mostelements) mostelements = sepcode_lastentry;
	if(window_lastentry > mostelements) mostelements = window_lastentry;
	for(count = 0; count < mostelements; count++) {
		if(count < sepcode_lastentry) {
			labels_separator[count] = master_separator[count];
			labels_escape[count] = master_separator[count];
			labels_newline[count] = master_separator[count];
		};
	};
	sprintf(maintitle, "%s", wintitles[window_main]);
	argmsg = (struct WBStartup *)argV;
	colinfolist[0] = version;
	colinfolist[1] = NULL;
	strings[string_blankentry] = blank_entry;
	labels_separator[sepcode_lastentry] = NULL;
	labels_escape[sepcode_lastentry] = NULL;
	labels_newline[sepcode_lastentry] = NULL;
	DoMethod(app, MUIM_Application_Load, MUIV_Application_Save_ENV);
	return(ecode);
};
///

APTR application() {

/// Main Window
    static char *Pages[10];

	lists[list_csv] = NListObject, MUIA_NList_Title, "Setlist-FM-dl",
		MUIA_NList_CompareHook, &compareHook,
		MUIA_NList_DisplayHook, &nlistHook,
	End;
	listviews[list_csv] = NListviewObject,
		MUIA_Background, MUII_ListBack,
		MUIA_Frame, MUIV_Frame_InputList,
		MUIA_NListview_NList, lists[list_csv],
	End;
	titleslider = SliderObject,
		MUIA_Slider_Horiz, TRUE,
		MUIA_Slider_Max, 0,
		MUIA_Slider_Level, 0,
	End;
	groups[group_toolbar] = GroupObject,
		Child, ColGroup(3),
			Child, Label2("      Setlist.fm User Name:"),
			Child, STR_tag = BetterStringObject, StringFrame, MUIA_CycleChain, TRUE,
            MUIA_ShortHelp, "Enter a Setlist.fm User Name and click Download User Shows!:",  End,
            Child, HGroup,
                Child, clear_tag = MUI_MakeObject(MUIO_Button, "_Clear User"), MUIA_CycleChain, TRUE,
                MUIA_ShortHelp, "Clear a Setlist.fm User Name:",
		    End,
        End,
	End;
	groups[group_mainview] = GroupObject,
		Child, listviews[list_csv], MUIA_ShortHelp, ".csv or .xml output will be displayed here",
		Child, HGroup, MUIA_Group_SameSize, TRUE,
			Child, HSpace(5),
                Child, HGroup,
                    Child, grab_gad = MUI_MakeObject(MUIO_Button, "_Download User Shows!"), MUIA_CycleChain, TRUE,
                    MUIA_ShortHelp, "Download a Setlist.fm User's shows in .csv format",
                End,
                Child, HGroup,
                    Child, grab_search = MUI_MakeObject(MUIO_Button, "_Search Setlist.fm"), MUIA_CycleChain, TRUE,
                    MUIA_ShortHelp, "Search Setlist.fm...",
			    End,
            Child, HSpace(5),
        End,
        Child, VSpace(3),
        Child, VGroup,
            Child, HGroup,
            Child, HSpace(0),
                Child, ColGroup(1),
                    Child, main_setlistfm_html = HyperlinkObject,
                    MUIA_Hyperlink_Text, "Powered by Setlist.fm API", MUIA_Hyperlink_URI,"https://www.setlist.fm",
                    End,
                End,
            End,
        End,
	End;
	groups[group_main] = VGroup,
		Child, GroupObject, MUIA_Background, MUII_GroupBack,
			Child, groups[group_toolbar],
			Child, groups[group_mainview],
		End,
	End;

    // Setlist.fm Search Tabs
    Pages[0] = " Setlists ",
    Pages[1] = " Artists ",
    Pages[2] = " Cities ",
    Pages[3] = " Venues ",
    Pages[4] = " User Attended ",
    Pages[5] = " User Edited ",
    Pages[6] = " User ID ",
    Pages[7] = " Countries ",
    Pages[8] = NULL,

	windows[window_main] = WindowObject,
		MUIA_Window_Title, maintitle,
		MUIA_Window_ID , MAKE_ID('M','A','I','N'),
		MUIA_Window_AppWindow, TRUE,
        MUIA_Window_Menustrip, MUI_MakeObject(MUIO_MenustripNM, MenuData1, 0),
		WindowContents, groups[group_main], End;
		return(ApplicationObject,
		MUIA_Application_Title, "Setlist-FM-dl",
		MUIA_Application_Version, version,
		MUIA_Application_Copyright, "©2020 IKE",
		MUIA_Application_Author, "IKE",
		MUIA_Application_Description, "Connects to Setlist.fm and downloads data\n",
		MUIA_Application_Base, "Setlist-FM-dl",
		SubWindow, windows[window_main],
///

/// Settings SubWindow
    SubWindow, win_settings = WindowObject, MUIA_Window_Title, "Settings", MUIA_Window_ID, MAKE_ID('S','T','N','G'),
		WindowContents, VGroup,
			Child, ColGroup(2), GroupFrame,
				Child, Label2("x-api-key:"),
				Child, STR_xapikeysetting = BetterStringObject, StringFrame, MUIA_ExportID, 1, //MUIA_ObjectID is the new version...
				MUIA_ShortHelp, "https://wwww.setlist.fm/settings/api", End,
                Child, Label2("Accept-Language:"),
				Child, STR_langsetting = BetterStringObject, StringFrame, MUIA_ExportID, 2, //MUIA_ObjectID is the new version...
                MUIA_ShortHelp, "You can provide any of the languages Spanish (es), French (fr), German (de), Portuguese (pt), Turkish (tr), Italian (it) or Polish (pl).  Just enter the two letter language code", End,
            End,
            Child, HGroup, MUIA_Group_SameSize, TRUE,
				Child, but_save   = MUI_MakeObject(MUIO_Button, "_Save"),
				Child, but_use    = MUI_MakeObject(MUIO_Button, "_Use"),
				Child, but_cancel = MUI_MakeObject(MUIO_Button, "_Cancel"),
			End,
            Child, VSpace(3),
            Child, VGroup,
                Child, HGroup,
                Child, HSpace(0),
                    Child, ColGroup(1),
                        Child, apikey_setlistfm_html = HyperlinkObject,
                        MUIA_Hyperlink_Text, "Get an API key", MUIA_Hyperlink_URI,"https://www.setlist.fm/settings/api",
                        End,
                    End,
                End,
            End,
        End,
	End,
///

// Setlist.fm Search SubWindow
    SubWindow, win_setlistfm = WindowObject, MUIA_Window_Title, "Search Setlist.fm", MUIA_Window_ID, MAKE_ID('S','R','C','H'),
        WindowContents, VGroup,
            Child, RegisterGroup(Pages),

/// Setlists Tab
                Child, VGroup,
                    Child, HGroup, GroupFrameT("Setlist Search"), ButtonFrame,
				        Child, VGroup,
                            Child, HGroup,
                                Child, Label2("Setlist:"),
					            Child, STR_setlists = BetterStringObject, StringFrame, MUIA_CycleChain, TRUE,
                                MUIA_ShortHelp, "Enter the Setlist you want to search: \nuse '%20' where spaces are needed...\n an example search: The%20Who", End,
				            End,
                            Child, HGroup,
                                Child, Label2("Page #:"),
                                Child, STR_setlists_page = SliderObject, MUIA_CycleChain, TRUE,
                                    MUIA_Numeric_Min , 1,
                                    MUIA_Numeric_Max , 500,
                                    MUIA_Numeric_Value, 1,
                                    MUIA_ShortHelp, "Enter the Page Number you want to search:",
                                End,		  		
                                Child, HGroup,
                                    Child, HGroup,
                                        Child, Label2("Auto convert .xml to .html"),
                                        Child, check_setlists_convert = MakeCheck(FALSE),
                                        MUIA_ShortHelp, "Automatically convert selected page\n from .xml to .html when Search is pressed...",
                                    End,
                                End,
                            End,
                            Child, HGroup,
                                Child, Label2("(optional) Date:"),
					            Child, STR_setlists_date = BetterStringObject, StringFrame, MUIA_CycleChain, TRUE,
                                MUIA_ShortHelp, "(Optional) Enter a Date you want\n to search: 'dd-MM-yyyy' ex. 20-08-1985", End,
                                Child, Label2("(optional) Tour Name:"),
					            Child, STR_setlists_tourname = BetterStringObject, StringFrame, MUIA_CycleChain, TRUE,
                                MUIA_ShortHelp, "(Optional) Enter the Tour Name you want to search...", End,
		  		            End,
                            Child, HGroup, MUIA_Group_SameSize, FALSE,
                                Child, HGroup,
					                Child, but_setlists_search = MUI_MakeObject(MUIO_Button, "_Search"), MUIA_CycleChain, TRUE,
                                End,
                            End,
                            Child, VSpace(4),
                            Child, MUI_MakeObject(MUIO_HBar, 4),
                            Child, VSpace(4),
                                Child, HGroup, GroupFrameT("Convert .xml to .html and save to Ram:?"), ButtonFrame,
                                    Child, HGroup,
                	                    Child, but_setlists_yes = MUI_MakeObject(MUIO_Button, "_Yes"), MUIA_CycleChain, TRUE,
					                End,
                                    Child, HGroup,
                                        Child, setlists_html_result = HyperlinkObject, MUIA_Frame, MUIV_Frame_Button, MUIA_CycleChain, TRUE,
                                        MUIA_Hyperlink_Text, "View .html", MUIA_Hyperlink_URI,"file:///Ram:setlists.html",
                                    End,
                                End,
                            End, 
                            Child, VSpace(5),
                            Child, HGroup, GroupFrameT("Results"), ButtonFrame,
                                Child, HGroup,
                                    Child, STR_setlists_html = BetterStringObject, MUIA_BetterString_NoInput, StringFrame,
                                End,
                            End,
                        End,
                    End,
                End,
                Child, VSpace(3),
                Child, VGroup,
                    Child, HGroup,
                    Child, HSpace(0),
                        Child, ColGroup(1),
                            Child, setlists_setlistfm_html = HyperlinkObject,
                            MUIA_Hyperlink_Text, "Powered by Setlist.fm API", MUIA_Hyperlink_URI,"https://www.setlist.fm",
                        End,
                    End,
                End,
            End,
        End,
///

/// Artists Tab
                Child, VGroup,
                    Child, HGroup, GroupFrameT("Artist Search"), ButtonFrame,
                        Child, VGroup,
                            Child, HGroup,
                                Child, Label2("Artist:"),
					            Child, STR_artists = BetterStringObject, StringFrame, MUIA_CycleChain, TRUE,
                                MUIA_ShortHelp, "Enter the Artist you want to search: \nuse '%20' where spaces are needed...\n an example search: The%20Who", End,
				            End,
                            Child, HGroup,
                                Child, Label2("Page #:"),
                                Child, STR_artists_page = SliderObject, MUIA_CycleChain, TRUE,
                                    MUIA_Numeric_Min , 1,
                                    MUIA_Numeric_Max , 500,
                                    MUIA_Numeric_Value, 1,
                                    MUIA_ShortHelp, "Enter the Page Number you want to search:",
                                End,
                                Child, HGroup,
                                    Child, HGroup,
                                        Child, Label2("Sort Artists by Name"),
                                        Child, check_artists = MakeCheck(FALSE),
                                        MUIA_ShortHelp, "Sort artists by Name instead of by Relevance...",
                                    End,
                                    Child, HGroup,
                                        Child, Label2("Auto convert .xml to .html"),
                                        Child, check_artists_convert = MakeCheck(FALSE),
                                        MUIA_ShortHelp, "Automatically convert selected page\n from .xml to .html when Search is pressed...",
                                    End,
                                End,
                            End,
                            Child, HGroup, MUIA_Group_SameSize, FALSE,
                                Child, HGroup,
					                Child, but_artists_search = MUI_MakeObject(MUIO_Button, "_Search"), MUIA_CycleChain, TRUE,
                                    MUIA_ShortHelp, "Default Search is sorted by Relevance...",
                                End,
                            End,
                            Child, VSpace(4),
                            Child, MUI_MakeObject(MUIO_HBar, 4),
                            Child, VSpace(4),
                                Child, HGroup, GroupFrameT("Convert .xml to .html and save to Ram:?"), ButtonFrame,
                                    Child, HGroup,
                	                    Child, but_artists_yes = MUI_MakeObject(MUIO_Button, "_Yes"), MUIA_CycleChain, TRUE,
					                    MUIA_ShortHelp, "Convert .xml to .html...",
                                    End,
                                    Child, HGroup,
                                        Child, artists_html_result = HyperlinkObject, MUIA_Frame, MUIV_Frame_Button, MUIA_CycleChain, TRUE,
                                        MUIA_Hyperlink_Text, "View .html", MUIA_Hyperlink_URI,"file:///Ram:artists.html",
                                    End,
                                End,
                            End,
                            Child, VSpace(5),
                            Child, HGroup, GroupFrameT("Results"), ButtonFrame,
                                Child, HGroup,
                                    Child, STR_artists_html = BetterStringObject, MUIA_BetterString_NoInput, StringFrame,
                                End,
                            End,
                        End,
                    End,
                End,
                Child, VSpace(3),
                Child, VGroup,
                    Child, HGroup,
                    Child, HSpace(0),
                        Child, ColGroup(1),
                            Child, artists_setlistfm_html = HyperlinkObject,
                            MUIA_Hyperlink_Text, "Powered by Setlist.fm API", MUIA_Hyperlink_URI,"https://www.setlist.fm",
                        End,
                    End,
                End,
            End,
        End,
///

/// Cities Tab
                Child, VGroup,
                    Child, HGroup, GroupFrameT("City Search"), ButtonFrame,
                        Child, VGroup,
                            Child, HGroup,
                                Child, Label2("City:"),
					            Child, STR_cities = BetterStringObject, StringFrame, MUIA_CycleChain, TRUE,
                                MUIA_ShortHelp, "Enter the City you want to search:\nuse '%20' where spaces are needed...\n an example search: Los%20Angeles", End,
				            End,
                            Child, HGroup,
                                Child, Label2("Page #:"),
                                Child, STR_cities_page = SliderObject, MUIA_CycleChain, TRUE,
                                    MUIA_Numeric_Min , 1,
                                    MUIA_Numeric_Max , 50,
                                    MUIA_Numeric_Value, 1,
                                    MUIA_ShortHelp, "Enter the Page Number you want to search:",
                                End,
                                Child, HGroup,
                                    Child, HGroup,
                                        Child, Label2("Auto convert .xml to .html"),
                                        Child, check_cities_convert = MakeCheck(FALSE),
                                        MUIA_ShortHelp, "Automatically convert selected page\n from .xml to .html when Search is pressed...",
                                    End,
                                End,
		  		            End,
                            Child, HGroup, MUIA_Group_SameSize, FALSE,
					            Child, HGroup,
									Child, but_cities_search = MUI_MakeObject(MUIO_Button, "_Search"), MUIA_CycleChain, TRUE,
                                End,
							End,
                            Child, VSpace(4),
                            Child, MUI_MakeObject(MUIO_HBar, 4),
                            Child, VSpace(4),
                                Child, HGroup, GroupFrameT("Convert .xml to .html and save to Ram:?"), ButtonFrame,
                                    Child, HGroup,
                	                    Child, but_cities_yes = MUI_MakeObject(MUIO_Button, "_Yes"), MUIA_CycleChain, TRUE,
								    End,
                                    Child, HGroup,
                                        Child, cities_html_result = HyperlinkObject, MUIA_Frame, MUIV_Frame_Button, MUIA_CycleChain, TRUE,
                                        MUIA_Hyperlink_Text, "View .html", MUIA_Hyperlink_URI,"file:///Ram:cities.html",
                                    End,
                                End,
                            End,
                            Child, VSpace(5),
                            Child, HGroup, GroupFrameT("Results"), ButtonFrame,
                                Child, HGroup,
                                    Child, STR_cities_html = BetterStringObject, MUIA_BetterString_NoInput, StringFrame, MUIA_CycleChain, TRUE,
                                End,
                            End,
                        End,
                    End,
                End,
                Child, VSpace(3),
                    Child, VGroup,
                        Child, HGroup,
                        Child, HSpace(0),
                            Child, ColGroup(1),
                                Child, cities_setlistfm_html = HyperlinkObject, 
                                MUIA_Hyperlink_Text, "Powered by Setlist.fm API", MUIA_Hyperlink_URI,"https://www.setlist.fm",
                            End,
                        End,
                    End,
                End,
            End,
///

/// Venues Tab
                Child, VGroup,
                    Child, HGroup, GroupFrameT("Venue Search"), ButtonFrame,
				        Child, VGroup,
                            Child, HGroup,
                            	Child, Label2("Venue:"),
					        	Child, STR_venues = BetterStringObject, StringFrame, MUIA_CycleChain, TRUE,
                            	MUIA_ShortHelp, "Enter the Venue (or City) you want to search: \nuse '%20' where spaces are needed...\n an example search: The%20Vogue", End,
				        	End,
                        	Child, HGroup,
                            	Child, Label2("Page #:"),
                                Child, STR_venues_page = SliderObject, MUIA_CycleChain, TRUE,
                                    MUIA_Numeric_Min , 1,
                                    MUIA_Numeric_Max , 250,
                                    MUIA_Numeric_Value, 1,
                                    MUIA_ShortHelp, "Enter the Page Number you want to search:",
                                End,
                                Child, HGroup,
                                    Child, HGroup,
                                        Child, Label2("Auto convert .xml to .html"),
                                        Child, check_venues_convert = MakeCheck(FALSE),
                                        MUIA_ShortHelp, "Automatically convert selected page\n from .xml to .html when Search is pressed...",
                                    End,
                                End,
		  		        	End,
                        	Child, HGroup, MUIA_Group_SameSize, FALSE,
					        	Child, HGroup,
									Child, but_venues_search = MUI_MakeObject(MUIO_Button, "_Search"), MUIA_CycleChain, TRUE,
								End,
							End,
                            Child, VSpace(4),
                            Child, MUI_MakeObject(MUIO_HBar, 4),
                            Child, VSpace(4),
                			    Child, HGroup, GroupFrameT("Convert .xml to .html and save to Ram:?"), ButtonFrame,
                        		    Child, HGroup,
                	        		    Child, but_venues_yes = MUI_MakeObject(MUIO_Button, "_Yes"), MUIA_CycleChain, TRUE,
					                End,
                    			    Child, HGroup,
                        			    Child, venues_html_result = HyperlinkObject, MUIA_Frame, MUIV_Frame_Button, MUIA_CycleChain, TRUE,
                        			    MUIA_Hyperlink_Text, "View .html", MUIA_Hyperlink_URI,"file:///Ram:venues.html",
                    			    End,
                                End,
                    		End,
                			Child, VSpace(5),
                				Child, HGroup, GroupFrameT("Results"), ButtonFrame,
                    				Child, HGroup,
                        				Child, STR_venues_html = BetterStringObject, MUIA_BetterString_NoInput, StringFrame, MUIA_CycleChain, TRUE,
                    				End,
            	  		        End,
           			        End,
        		        End,
        	        End,
        	        Child, VSpace(3),
        	        Child, VGroup,
            	        Child, HGroup,
                        Child, HSpace(0),
                            Child, ColGroup(1),
                                Child, venues_setlistfm_html = HyperlinkObject,
                                MUIA_Hyperlink_Text, "Powered by Setlist.fm API", MUIA_Hyperlink_URI,"https://www.setlist.fm",
                            End,
                        End,
                    End,
                End,
            End,
///

/// User Attended Tab
                Child, VGroup,
                	Child, HGroup, GroupFrameT("User Attended Search"), ButtonFrame,
				    	Child, VGroup,
                        	Child, HGroup,
                            	Child, Label2("Enter User ID:"),
					        	Child, STR_userid_attended = BetterStringObject, StringFrame, MUIA_CycleChain, TRUE,
                            	MUIA_ShortHelp, "Enter the User ID you want to search:", End,
				        	End,
                        	Child, HGroup,
                            	Child, Label2("Page #:"),
                                Child, STR_userid_attended_page = SliderObject, MUIA_CycleChain, TRUE,
                                    MUIA_Numeric_Min , 1,
                                    MUIA_Numeric_Max , 500,
                                    MUIA_Numeric_Value, 1,
                                    MUIA_ShortHelp, "Enter the Page Number you want to search:",
                                End,
                                Child, HGroup,
                                    Child, HGroup,
                                        Child, Label2("Auto convert .xml to .html"),
                                        Child, check_userid_attended_convert = MakeCheck(FALSE),
                                        MUIA_ShortHelp, "Automatically convert selected page\n from .xml to .html when Search is pressed...",
                                    End,
                                End,
                            End,
                        	Child, HGroup, MUIA_Group_SameSize, FALSE,
								Child, HGroup,
					        		Child, but_userid_attended_search = MUI_MakeObject(MUIO_Button, "_Search"), MUIA_CycleChain, TRUE,
                            	End,
							End,
                            Child, VSpace(4),
                            Child, MUI_MakeObject(MUIO_HBar, 4),
                            Child, VSpace(4),
                			    Child, HGroup, GroupFrameT("Convert .xml to .html and save to Ram:?"), ButtonFrame,
                        		    Child, HGroup,
                	        		    Child, but_userid_attended_yes = MUI_MakeObject(MUIO_Button, "_Yes"), MUIA_CycleChain, TRUE,
								    End,
                    			    Child, HGroup,
                        			    Child, userid_attended_html_result = HyperlinkObject, MUIA_Frame, MUIV_Frame_Button, MUIA_CycleChain, TRUE,
                        			    MUIA_Hyperlink_Text, "View .html", MUIA_Hyperlink_URI,"file:///Ram:attended.html",
                    			    End,
				        		End,
                    		End,
                			Child, VSpace(5),
                			Child, HGroup, GroupFrameT("Results"), ButtonFrame,
                    			Child, HGroup,
                        			Child, STR_userid_attended_html = BetterStringObject, MUIA_BetterString_NoInput, StringFrame,
                    			End,
            			    End,
        			    End,
     			    End,
    		    End,
        	    Child, VSpace(3),
        	    Child, VGroup,
            	    Child, HGroup,
                        Child, HSpace(0),
                	    Child, ColGroup(1),
                            Child, userid_attended_setlistfm_html = HyperlinkObject,
                            MUIA_Hyperlink_Text, "Powered by Setlist.fm API", MUIA_Hyperlink_URI,"https://www.setlist.fm",
                        End,
                    End,
                End,
            End,
        End,
///

/// User Edited Tab
                Child, VGroup,
                    Child, HGroup, GroupFrameT("User Edited Search"), ButtonFrame,
                    	Child, VGroup,
                        	Child, HGroup,
                            	Child, Label2("Enter User ID:"),
					        	Child, STR_userid_edited = BetterStringObject, StringFrame, MUIA_CycleChain, TRUE,
                            	MUIA_ShortHelp, "Enter the User ID you want to search:", End,
				        	End,
                        	Child, HGroup,
                            	Child, Label2("Page #:"),                 
                                Child, STR_userid_edited_page = SliderObject, MUIA_CycleChain, TRUE, 
                                    MUIA_Numeric_Min , 1,
                                    MUIA_Numeric_Max , 4000,
                                    MUIA_Numeric_Value, 1,
                                    MUIA_ShortHelp, "Enter the Page Number you want to search:",
                                End,
                                Child, HGroup,
                                    Child, HGroup,
                                        Child, Label2("Auto convert .xml to .html"),
                                        Child, check_userid_edited_convert = MakeCheck(FALSE),
                                        MUIA_ShortHelp, "Automatically convert selected page\n from .xml to .html when Search is pressed...",
                                    End,
                                End,
		  		        	End,
                        	Child, HGroup, MUIA_Group_SameSize, FALSE,
								Child, HGroup,
					        		Child, but_userid_edited_search = MUI_MakeObject(MUIO_Button, "_Search"), MUIA_CycleChain, TRUE,
                            	End,
							End,
                            Child, VSpace(4),
                            Child, MUI_MakeObject(MUIO_HBar, 4),
                            Child, VSpace(4),
                    		    Child, HGroup, GroupFrameT("Convert .xml to .html and save to Ram:?"), ButtonFrame,
                            	    Child, HGroup,
                	            	    Child, but_userid_edited_yes = MUI_MakeObject(MUIO_Button, "_Yes"), MUIA_CycleChain, TRUE,
								    End,
                    			    Child, HGroup,
                        			    Child, userid_edited_html_result = HyperlinkObject, MUIA_Frame, MUIV_Frame_Button, MUIA_CycleChain, TRUE,
                        			    MUIA_Hyperlink_Text, "View .html", MUIA_Hyperlink_URI,"file:///Ram:edited.html",
                    			    End,
				            	End,
                        	End,
                			Child, VSpace(5),
                			Child, HGroup, GroupFrameT("Results"), ButtonFrame,
                    			Child, HGroup,
                        			Child, STR_userid_edited_html = BetterStringObject, MUIA_BetterString_NoInput, StringFrame,
                    			End,
            			    End,
        			    End,
     			    End,
   			    End,
        		Child, VSpace(3),
            	Child, VGroup,
                	Child, HGroup,
                    	Child, HSpace(0),
                        	Child, ColGroup(1),
                            	Child, userid_edited_setlistfm_html = HyperlinkObject,
                            	MUIA_Hyperlink_Text, "Powered by Setlist.fm API", MUIA_Hyperlink_URI,"https://www.setlist.fm",
                        	End,
                    	End,
                	End,
            	End,
    		End,
///

/// UserID Tab
                Child, VGroup,
                	Child, HGroup, GroupFrameT("User ID Search"), ButtonFrame,
				    	Child, VGroup,
                        	Child, HGroup,
                            	Child, Label2("Enter User ID:"),
					        	Child, STR_userid = BetterStringObject, StringFrame, MUIA_CycleChain, TRUE,
                            	MUIA_ShortHelp, "Enter the User ID you want to search:", End,
                            Child, HGroup,
                                Child, HGroup,
                                    Child, Label2("Auto convert .xml to .html"),
                                    Child, check_userid_convert = MakeCheck(FALSE),
                                    MUIA_ShortHelp, "Automatically convert selected page\n from .xml to .html when Search is pressed...",
                                End,
                            End,
                        End,
                        Child, HGroup, MUIA_Group_SameSize, FALSE,
							Child, HGroup,
					        	Child, but_userid_search = MUI_MakeObject(MUIO_Button, "_Search"), MUIA_CycleChain, TRUE,
                            End,
							Child, HGroup,
								Child, but_userid_clear = MUI_MakeObject(MUIO_Button, "_Clear User ID"), MUIA_CycleChain, TRUE,
			            	End,
                    	End,
                	End,
                End,
                Child, VSpace(4),
                Child, MUI_MakeObject(MUIO_HBar, 4),
                Child, VSpace(4),
                Child, HGroup, GroupFrameT("Convert .xml to .html and save to Ram:?"), ButtonFrame,
                    Child, HGroup,
                	    Child, but_userid_yes = MUI_MakeObject(MUIO_Button, "_Yes"), MUIA_CycleChain, TRUE,
				    End,
                    Child, HGroup,
                        Child, userid_html_result = HyperlinkObject, MUIA_Frame, MUIV_Frame_Button, MUIA_CycleChain, TRUE,
                        MUIA_Hyperlink_Text, "View .html", MUIA_Hyperlink_URI,"file:///Ram:user.html",
                    End,
				 End,
              End,
              Child, VSpace(5),
                  Child, HGroup, GroupFrameT("Results"), ButtonFrame,
                      Child, HGroup,
                          Child, STR_userid_html = BetterStringObject, MUIA_BetterString_NoInput, StringFrame,
                      End,
            	  End,
        	  End,
        	  Child, VSpace(3),
        	      Child, VGroup,
            	      Child, HGroup,
                          Child, HSpace(0),
                          Child, ColGroup(1),
                              Child, userid_setlistfm_html = HyperlinkObject,
                              MUIA_Hyperlink_Text, "Powered by Setlist.fm API", MUIA_Hyperlink_URI,"https://www.setlist.fm",
                          End,
                      End,
                  End,
              End,
          End,
///

/// Countries Tab
                Child, VGroup,
                    Child, HGroup, GroupFrameT("Country Search"), ButtonFrame,
                        Child, Label2("Auto convert .xml to .html"),
                        Child, check_countries_convert = MakeCheck(FALSE),
                        MUIA_ShortHelp, "Automatically convert selected page\n from .xml to .html when Search is pressed...",
                        Child, HGroup,
                            Child, but_countries = MUI_MakeObject(MUIO_Button, "_Search"), MUIA_CycleChain, TRUE,
                        End,
                    End,
                    Child, VSpace(4),
                    Child, MUI_MakeObject(MUIO_HBar, 4),
                    Child, VSpace(4),
              		Child, VGroup,
                		Child, HGroup, GroupFrameT("Convert .xml to .html and save to Ram:?"), ButtonFrame,
                        	Child, HGroup,
                	        	Child, but_countries_yes = MUI_MakeObject(MUIO_Button, "_Yes"), MUIA_CycleChain, TRUE,
							End,
                                Child, countries_html_result = HyperlinkObject, MUIA_Frame, MUIV_Frame_Button, MUIA_CycleChain, TRUE,
                                MUIA_Hyperlink_Text, "View .html", MUIA_Hyperlink_URI,"file:///Ram:countries.html",
				        	End,
            			End,
                	End,
                	Child, VSpace(5),
                   	Child, VGroup,
                        Child, HGroup, GroupFrameT("Results"), ButtonFrame,
                            Child, HGroup,
                                Child, STR_countries_html = BetterStringObject, MUIA_BetterString_NoInput, StringFrame,
                            End,
                        End,
                    End,
                    Child, VSpace(3),
                    Child, VGroup,
                        Child, HGroup,
                            Child, HSpace(0),
                                Child, ColGroup(1),
                                    Child, countries_setlistfm_html = HyperlinkObject,
                                    MUIA_Hyperlink_Text, "Powered by Setlist.fm API", MUIA_Hyperlink_URI,"https://www.setlist.fm",
                                End,
                            End,
                        End,
                    End,
         	    End,
     	    End,
///
            End,
        End,
    End,
End);
};

// Main program
int main(int argc, char *argv[]) {
    ULONG signals = 0;

	if (! Open_Libs(TRUE)) {
        printf("MUI Library error...\n"); 
        return(0);
	}
	if(!initdata(argc, argv)) app = application();
	
    if (!app) {
        printf("MUI Application error...\n");
        return(0);
	}

    notify(group_main, TRUE);

    while(running) {
        ULONG id = DoMethod(app, MUIM_Application_Input, &signals);

		switch(id) {

/// Main interface
			case GRAB:
                xapikey_var = "x-api-key:";
                strcat(xapikey_var, xapikey);
                langcode_var = "Accept-Language: ";
                strcat(langcode_var, langcode);

            	get(STR_tag, MUIA_String_Contents, &tag);

                strcpy(host, "https://backup-setlistfm.herokuapp.com/export.csv?name=");
	            strcat(host, tag);

				get_setlistapi();
                rename(filename, setlistfm_filename);
				break;

			case GRABCLEAR:
				set(STR_tag, MUIA_String_Contents, 0);
				break;

            case GRABSEARCH:
                set(win_setlistfm, MUIA_Window_Open, TRUE);
                break;
///

/// Project Menu items
			case MEN_ABOUT:
				MUI_Request(app, windows[window_main], 0, "About Setlist-FM-dl", "*OK", about_text, NULL);
				break;

			case MEN_ABOUTMUI:
				if(!aboutwin) {
					aboutwin = AboutmuiObject, MUIA_Aboutmui_Application, app, End;
				}
                if(aboutwin)
					set(aboutwin, MUIA_Window_Open, TRUE);
				else
					DisplayBeep(0);
				break;

			case MEN_QUIT:
			case MUIV_Application_ReturnID_Quit:
				running = FALSE;
				break;
///

/// Setlist.fm Search
            case MEN_SETLISTFM:
                set(win_setlistfm, MUIA_Window_Open, TRUE);
                break;
///

/// Setlists search
            case SETLISTS:
                STR_page_number = STR_setlists_page;
                get_PageNumber();
                xapikey_var = "x-api-key:";
                strcat(xapikey_var, xapikey);
                langcode_var = "Accept-Language: ";
                strcat(langcode_var, langcode);
                get(STR_setlists, MUIA_String_Contents, &setlists);
                get(STR_setlists_date, MUIA_String_Contents, &setlists_date);
                get(STR_setlists_tourname, MUIA_String_Contents, &setlists_tourname);

                /* search/setlists - uses setlists.xml */
                strcpy(host, "https://api.setlist.fm/rest/1.0/search/setlists?artistName="); // cityName= countryCode= stateCode= venueName= year=
                strcat(host, setlists);
                strcat(host, "&p=");
                strcat(host, str);  
                strcat(host, "&date=");
                strcat(host, setlists_date);
                strcat(host, "&tourName=");
                strcat(host, setlists_tourname);

                get_setlistapi();
                  set(STR_setlists_html, MUIA_String_Contents, "Check 'Ram:' for output file...");

                      if (getbool(check_setlists_convert)){

                          setlistapi_html = "Ram:setlists.html";
                          setlistapi_xml = "xml/setlists.xml";
                          get_setlistapi_html();
                          set(STR_setlists_html, MUIA_String_Contents, "Check 'Ram:' for .html result...");
                      }
                      else{
                          break;
                      }
                break;

            case YES_SETLISTS:
                setlistapi_html = "Ram:setlists.html";
                setlistapi_xml = "xml/setlists.xml";
                get_setlistapi_html();
                set(STR_setlists_html, MUIA_String_Contents, "Check 'Ram:' for .html result...");
				break;
///

/// Artists Search
            case ARTISTS:

	            if (getbool(check_artists)){

                    set(STR_artists_html, MUIA_String_Contents, "Sorting by Name...");

                    STR_page_number = STR_artists_page;
                    get_PageNumber();
                    xapikey_var = "x-api-key:";
                    strcat(xapikey_var, xapikey);
                    langcode_var = "Accept-Language: ";
                    strcat(langcode_var, langcode);
                    get(STR_artists, MUIA_String_Contents, &artists);

                    /* search/artist - uses artists.xml */
                    strcpy(host, "https://api.setlist.fm/rest/1.0/search/artists?artistName=");
                    strcat(host, artists);
                    strcat(host, "&p=");
                    strcat(host, str);
                    strcat(host, "&sort=sortName");

                    get_setlistapi();
                    set(STR_artists_html, MUIA_String_Contents, "Check 'Ram:' for output file...");

                        if (getbool(check_artists_convert)){

                            setlistapi_html = "Ram:artists.html";
                            setlistapi_xml = "xml/artists.xml";
                            get_setlistapi_html();
                            set(STR_artists_html, MUIA_String_Contents, "Check 'Ram:' for .html result...");

                       }
                        else{
                             break;
                            }
                    break;
	            }else
                                                          
                    set(STR_artists_html, MUIA_String_Contents, "Sorting by Relevance... ");

                    STR_page_number = STR_artists_page;
                    get_PageNumber();
                    xapikey_var = "x-api-key:";
                    strcat(xapikey_var, xapikey);
                    langcode_var = "Accept-Language: ";
                    strcat(langcode_var, langcode);
                    get(STR_artists, MUIA_String_Contents, &artists);

                    /* search/artist - uses artists.xml */
                    strcpy(host, "https://api.setlist.fm/rest/1.0/search/artists?artistName=");
                    strcat(host, artists);
                    strcat(host, "&p=");
                    strcat(host, str);
                    strcat(host, "&sort=relevance"); //sort=sortName (default)

                    get_setlistapi();
                    set(STR_artists_html, MUIA_String_Contents, "Check 'Ram:' for output file...");

                        if (getbool(check_artists_convert)){

                            setlistapi_html = "Ram:artists.html";
                            setlistapi_xml = "xml/artists.xml";
                            get_setlistapi_html();
                            set(STR_artists_html, MUIA_String_Contents, "Check 'Ram:' for .html result...");
                        }
                        else{
                             break;
                            }
                    break;

            case YES_ARTISTS:
                setlistapi_html = "Ram:artists.html";
                setlistapi_xml = "xml/artists.xml";
                get_setlistapi_html();
                set(STR_artists_html, MUIA_String_Contents, "Check 'Ram:' for .html result...");
				break;
///

/// Cities search
            case CITIES:
                STR_page_number = STR_cities_page;
                get_PageNumber();
                xapikey_var = "x-api-key:";
                strcat(xapikey_var, xapikey);
                langcode_var = "Accept-Language: ";
                strcat(langcode_var, langcode);
                get(STR_cities, MUIA_String_Contents, &cities);

                /* search/cities - uses cities.xml */
                strcpy(host, "https://api.setlist.fm/rest/1.0/search/cities?name="); // country= stateCode=
                strcat(host, cities);
                strcat(host, "&p=");
                strcat(host, str); 

                get_setlistapi();
                  set(STR_cities_html, MUIA_String_Contents, "Check 'Ram:' for output file...");

                      if (getbool(check_cities_convert)){

                          setlistapi_html = "Ram:cities.html";
                          setlistapi_xml = "xml/cities.xml";
                          get_setlistapi_html();
                          set(STR_cities_html, MUIA_String_Contents, "Check 'Ram:' for .html result...");
                      }
                      else{
                           break;
                      }
                break;

            case YES_CITIES:
                setlistapi_html = "Ram:cities.html";
                setlistapi_xml = "xml/cities.xml";
                get_setlistapi_html();
                set(STR_cities_html, MUIA_String_Contents, "Check 'Ram:' for .html result...");
				break;
///

/// Venues search
            case VENUES:
                STR_page_number = STR_venues_page;
                get_PageNumber();
                xapikey_var = "x-api-key:";
                strcat(xapikey_var, xapikey);
                langcode_var = "Accept-Language: ";
                strcat(langcode_var, langcode);
                get(STR_venues, MUIA_String_Contents, &venues);

                /* venues - search - uses venues.xml */
                strcpy(host, "https://api.setlist.fm/rest/1.0/search/venues?name="); // cityName= country= stateCode=
                strcat(host, venues);
                strcat(host, "&p=");
                strcat(host, str); 

                get_setlistapi();
                    set(STR_venues_html, MUIA_String_Contents, "Check 'Ram:' for output file...");

                       if (getbool(check_venues_convert)){

                           setlistapi_html = "Ram:venues.html";
                           setlistapi_xml = "xml/venues.xml";
                           get_setlistapi_html();
                           set(STR_venues_html, MUIA_String_Contents, "Check 'Ram:' for .html result...");
                       }
                       else{
                           break;
                       }
                break;

            case YES_VENUES:
                setlistapi_html = "Ram:venues.html";
                setlistapi_xml = "xml/venues.xml";
                get_setlistapi_html();
                set(STR_venues_html, MUIA_String_Contents, "Check 'Ram:' for .html result...");
				break;
///

/// UserID Attended search
            case USERIDATTENDED:
                STR_page_number = STR_userid_attended_page;
                get_PageNumber();
                xapikey_var = "x-api-key:";
                strcat(xapikey_var, xapikey);
                langcode_var = "Accept-Language: ";
                strcat(langcode_var, langcode);
                get(STR_userid_attended, MUIA_String_Contents, &userid_attended);

                /* user/attended - search uses attended.xml */
                strcpy(host, "https://api.setlist.fm/rest/1.0/user/");
                strcat(host, userid_attended);
                strcat(host, "/attended?p=");
                strcat(host, str); 

                get_setlistapi();
                set(STR_userid_attended_html, MUIA_String_Contents, "Check 'Ram:' for output file...");

                    if (getbool(check_userid_attended_convert)){

                        setlistapi_html = "Ram:attended.html";
                        setlistapi_xml = "xml/attended.xml";
                        get_setlistapi_html();
                        set(STR_userid_attended_html, MUIA_String_Contents, "Check 'Ram:' for .html result...");
                    }
                    else{
                         break;
                    }
                break;

            case YES_USERIDATTENDED:
                setlistapi_html = "Ram:attended.html";
                setlistapi_xml = "xml/attended.xml";
                get_setlistapi_html();
			    set(STR_userid_attended_html, MUIA_String_Contents, "Check 'Ram:' for .html result...");
            	break;
///

/// UserID Edited search
            case USEREDITED:
                STR_page_number = STR_userid_edited_page;
                get_PageNumber();
                xapikey_var = "x-api-key:";
                strcat(xapikey_var, xapikey);
                langcode_var = "Accept-Language: ";
                strcat(langcode_var, langcode);
                get(STR_userid_edited, MUIA_String_Contents, &userid_edited);

                /* user/edited - search uses edited.xml */
                strcpy(host, "https://api.setlist.fm/rest/1.0/user/");
                strcat(host, userid_edited);
                strcat(host, "/edited?p=");
                strcat(host, str);  

                get_setlistapi();
                    set(STR_userid_edited_html, MUIA_String_Contents, "Check 'Ram:' for output file...");

                        if (getbool(check_userid_edited_convert)){

                            setlistapi_html = "Ram:edited.html";
                            setlistapi_xml = "xml/edited.xml";
                            get_setlistapi_html();
                            set(STR_userid_edited_html, MUIA_String_Contents, "Check 'Ram:' for .html result...");
                        }
                        else{
                             break;
                        }
                break;

            case YES_USEREDITED:
                setlistapi_html = "Ram:edited.html";
                setlistapi_xml = "xml/edited.xml";
                get_setlistapi_html();
                set(STR_userid_edited_html, MUIA_String_Contents, "Check 'Ram:' for .html result...");
				break;
///

/// UserID search
            case USERID:
                xapikey_var = "x-api-key:";
                strcat(xapikey_var, xapikey);
                langcode_var = "Accept-Language: ";
                strcat(langcode_var, langcode);

                /* user id  - uses user.xml */
                strcpy(host, "https://api.setlist.fm/rest/1.0/user/");
                strcat(host, userid);

                get_setlistapi();
                    set(STR_userid_html, MUIA_String_Contents, "Check 'Ram:' for output file...");

                        if (getbool(check_userid_convert)){

                            setlistapi_html = "Ram:user.html";
                            setlistapi_xml = "xml/user.xml";
                            get_setlistapi_html();
                            set(STR_userid_html, MUIA_String_Contents, "Check 'Ram:' for .html result...");
                        }
                        else{
                             break;
                        }
                break;

            case CLEAR_USERID:
				set(STR_userid, MUIA_String_Contents, 0);
				break;

            case YES_USERID:
                setlistapi_html = "Ram:user.html";
                setlistapi_xml = "xml/user.xml";
                get_setlistapi_html();
                set(STR_userid_html, MUIA_String_Contents, "Check 'Ram:' for .html result...");
                break;
///

/// Countries search
            case BUT_COUNTRIES:
                xapikey_var = "x-api-key:";
                strcat(xapikey_var, xapikey);
                langcode_var = "Accept-Language: ";
                strcat(langcode_var, langcode);

                /* search/countries - uses countries.xml */
                strcpy(host, "https://api.setlist.fm/rest/1.0/search/countries");

                get_setlistapi();
                    set(STR_countries_html, MUIA_String_Contents, "Check 'Ram:' for output file...");

                        if (getbool(check_countries_convert)){

                            setlistapi_html = "Ram:countries.html";
                            setlistapi_xml = "xml/countries.xml";
                            get_setlistapi_html();
                            set(STR_countries_html, MUIA_String_Contents, "Check 'Ram:' for .html result...");
                        }
                        else{
                             break;
                        }
                break;

            case YES_COUNTRIES:
                setlistapi_html = "Ram:countries.html";
                setlistapi_xml = "xml/countries.xml";
                get_setlistapi_html();

                set(STR_countries_html, MUIA_String_Contents, "Check 'Ram:' for .html result...");
				break;
///

/// Settings Menu items
            case MEN_PREFS:
				set(win_settings, MUIA_Window_Open, TRUE);
				break;

  			case SAVE:
			    get(STR_xapikeysetting, MUIA_String_Contents, &xapikey); 
                get(STR_langsetting, MUIA_String_Contents, &langcode);

				DoMethod(app, MUIM_Application_Save, MUIV_Application_Save_ENVARC);
				set(win_settings, MUIA_Window_Open, FALSE);
				break;

			case USE:
	   		    get(STR_xapikeysetting, MUIA_String_Contents, &xapikey);
        	    get(STR_langsetting, MUIA_String_Contents, &langcode);

				DoMethod(app, MUIM_Application_Save, MUIV_Application_Save_ENV);
				break;

		    case CANCEL:
				set(win_settings, MUIA_Window_Open, FALSE);
				break;

			case MEN_DELETE:
		        remove(filename);
				break;

			case MEN_MUIPREFS:
				DoMethod(app, MUIM_Application_OpenConfigWindow, 0);
				break;
///

		}
		if (running && signals) Wait(signals);
	}
    notify(group_main, FALSE);
    MUI_DisposeObject(app);
	Open_Libs(FALSE);

    return 0;
}
