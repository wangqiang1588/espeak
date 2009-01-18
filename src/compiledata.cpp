/***************************************************************************
 *   Copyright (C) 2005 to 2007 by Jonathan Duddington                     *
 *   email: jonsd@users.sourceforge.net                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see:                                 *
 *               <http://www.gnu.org/licenses/>.                           *
 ***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "wx/wx.h"
#include "wx/wfstream.h"
#include "wx/dir.h"
#include "wx/filename.h"

#include "speak_lib.h"
#include "main.h"
#include "speech.h"
#include "options.h"
#include "phoneme.h"
#include "synthesize.h"
#include "voice.h"
#include "spect.h"

#ifdef PLATFORM_POSIX
#include <unistd.h>
#endif



#define tNUMBER   1
#define tSTRING   2
#define tPHONEMEMNEM  3
#define tKEYWORD  4
#define tSIGNEDNUMBER 5

#define tPHONEMESTART 1
#define tEND      2
#define tSPECT    3
#define tWAVE   4
#define tSTARTTYPE 5
#define tENDTYPE  6
#define tBEFORE   7
#define tAFTER    8
#define tTONESPEC 9
#define tLENGTHMOD 11
#define tLENGTH   12
#define tLONGLENGTH 13
#define tOLDNAME  14
#define tREDUCETO 15
#define tFIXEDCODE 16

#define tBEFOREVOWEL 17
#define tBEFOREVOWELPAUSE 18
#define tBEFORENOTVOWEL 19
#define tBEFORENOTVOWEL2 20
#define tSWITCHVOICING  21
#define tBEFORE_R 22

#define tLINKOUT  23
#define tVOWELIN  24
#define tVOWELOUT 25
#define tAPPENDPH 26         // always insert another phoneme (linkout) after this one
#define tIMPORTPH 27
#define tBEFOREPAUSE 28

#define tPHONEMENUMBER 29
#define tPHONEMETABLE  30
#define tINCLUDE  31
#define tSTRESSTYPE  32


static const int flags_alternative[] = {phBEFOREVOWEL,phBEFOREVOWELPAUSE,phBEFORENOTVOWEL,phBEFORENOTVOWEL2,phSWITCHVOICING,phBEFORE_R};

extern void MakeVowelLists(void);
extern void FindPhonemesUsed(void);
extern void DrawEnvelopes();
extern int LoadEnvelope2(FILE *f, const char *fname);

static int markers_used[8];

typedef struct {
	void *link;
	int value;
	int ph_mnemonic;
	short ph_table;
	char string[1];
} REF_HASH_TAB;


#define N_ENVELOPES  30
#define ENV_LEN  128
int n_envelopes = 0;
char envelope_paths[N_ENVELOPES][80];
unsigned char envelope_dat[N_ENVELOPES][ENV_LEN];
static FILE *f_phdata;



class Compile
{//==========
public:
	Compile();
	void CPhonemeTab(const char *path);

private:
	int CPhoneme();
	void StartPhonemeTable(const char *name);
	void EndPhonemeTable();
	void CPhonemeFiles();
	int NextItem(int type);
	void UngetItem();
	void Error(const char *msg);
	void Error(const char *msg, const char *msg2);
	void Summary(FILE *f);
	void SummarySpect(FILE *f,unsigned int value);
	void SummarySpectList(FILE *f,int ref,const char *label);
	const char *PhonemeMnem(int phcode);
	void WritePhonemeTable();
	void Report();
	
	int LookupPhoneme(const char *string, int declare);
	void ReservePhCode(const char *string, int value);
	const char *GetKeyMnem(int value);
	int LoadSpect(const char *path, int control);
	int LoadWavefile(FILE *f, const char *fname);
	int LoadEnvelope(FILE *f, const char *fname);
	int LoadDataFile(const char *path, int control);
	int AddSpect(int phcode, int *list, int control);
	void AddSpectList(int *list, int control);
	void AddEnvelope(int *list);
	void VowelTransition(int which, unsigned int *trans);
	void ImportPhoneme(void);


	FILE *f_in;
	FILE *f_phcontents;
	FILE *f_errors;
	FILE *f_phindex;
	FILE *f_phtab;
	int f_in_displ;
	int f_in_linenum;

	PHONEME_TAB *ph;

	int linenum;
	int item_type;
	char current_fname[80];
	char item_string[256];
	char phoneme_tab_name[N_PHONEME_TAB_NAME];
	unsigned int vowel_in[2];
	unsigned int vowel_out[2];

	int n_phcodes_list[N_PHONEME_TABS];
	PHONEME_TAB_LIST phoneme_tab_list2[N_PHONEME_TABS];
	PHONEME_TAB *phoneme_tab2;
	int n_phoneme_tabs;
	
	
	int n_phcodes;
	
	int count_references;
	int duplicate_references;
	int count_frames;
	int error_count;
	
	REF_HASH_TAB *ref_hash_tab[256];
	
	typedef struct {
		FILE *file;
		int  linenum;
		char fname[80];
	} STACK;
	
	#define N_STACK  12
	int stack_ix;
	STACK stack[N_STACK];

};

char path_source[sizeof(path_home)+20];

Compile *compile;

typedef struct {
	const char *mnem;
	int data;
} keywtab_t;

static keywtab_t keywords[] = {
	{"vowel",     0x1000000+phVOWEL},
	{"liquid",    0x1000000+phLIQUID},
	{"pause",     0x1000000+phPAUSE},
	{"stress",    0x1000000+phSTRESS},
	{"virtual",   0x1000000+phVIRTUAL},

	{"fricative", 0x1000000+phFRICATIVE},
	{"vstop",     0x1000000+phVSTOP},
	{"vfricative",0x1000000+phVFRICATIVE},
	{"delete_phoneme", 0x1000000+phDELETED},

	// type of consonant
	{"stop",      0x1000000+phSTOP},
	{"frc",       0x1000000+phFRICATIVE},
	{"nasal",     0x1000000+phNASAL},
	{"flp",       0x1000000+phVSTOP},
	{"afr",       0x1000000+phSTOP},      // treat as stop
	{"apr",       0x1000000+phFRICATIVE}, // [h] voiceless approximant


	// keywords
	{"phonemenumber", 29},
	{"phonemetable", 30},
	{"include", 31},

	{"phoneme",  1},
	{"endphoneme",  2},
	{"formants", 3},
	{"wave",     4},
	{"starttype",5},
	{"endtype",  6},
	{"before",   7},
	{"after",    8},
	{"tone",     9},
	{"lengthmod",11},
	{"length",   12},
	{"longlength", 13},
	{"reduceto", 15},
	{"beforevowel", tBEFOREVOWEL},
	{"beforevowelpause", tBEFOREVOWELPAUSE},
	{"beforenotvowel",tBEFORENOTVOWEL},
	{"beforenotvowel2",tBEFORENOTVOWEL2},
	{"before_r",tBEFORE_R},
	{"linkout",tLINKOUT},
	{"switchvoicing",tSWITCHVOICING},
	{"vowelin",tVOWELIN},
	{"vowelout",tVOWELOUT},
	{"appendph",tAPPENDPH},
	{"import_phoneme",tIMPORTPH},
	{"beforepause",tBEFOREPAUSE},
	{"stress_type",tSTRESSTYPE},

	// flags
	{"wavef",      0x2000000+phWAVE},
	{"unstressed", 0x2000000+phUNSTRESSED},
	{"fortis",     0x2000000+phFORTIS},
	{"sibilant",   0x2000000+phSIBILANT},
	{"nolink",     0x2000000+phNOLINK},
	{"trill",      0x2000000+phTRILL},
	{"vowel2",     0x2000000+phVOWEL2},
	{"palatal",    0x2000000+phPALATAL},
	{"long",       0x2000000+phLONG},
	{"brkafter",   0x2000000+phBRKAFTER},
	{"rhotic",     0x2000000+phRHOTIC},
	{"nonsyllabic",0x2000000+phNONSYLLABIC},
	{"lengthenstop",0x2000000+phLENGTHENSTOP},

	// voiced / unvoiced
	{"vcd",	0x2000000+phVOICED},
	{"vls",	0x2000000+phFORTIS},

	// place of articulation
	{"blb",	0x2010000},
	{"lbd",	0x2020000},
	{"dnt",	0x2030000},
	{"alv",	0x2040000},
	{"rfx",	0x2050000},
	{"pla",	0x2060000},
	{"pal",	0x2070000},
	{"vel",	0x2080000},
	{"lbv",	0x2090000},
	{"uvl",	0x20a0000},
	{"phr",	0x20b0000},
	{"glt",	0x20c0000},

	// vowel transition attributes
	{"len=",	0x3000001},
	{"rms=",	0x3000002},
	{"f1=",	0x3000003},
	{"f2=",	0x3000004},
	{"f3=",	0x3000005},
	{"brk",  0x3000006},
	{"rate", 0x3000007},
	{"glstop", 0x3000008},
	{"lenadd", 0x3000009},
	{"f4",   0x300000a},
	{"paus", 0x300000b},
	{"colr=",0x300000c},
	{"amp=", 0x300000d},  // set rms of 1st frame as fraction of rms of 2nd frame  (1/30ths)

	{"", -1}
};


extern void strncpy0(char *to,const char *from, int size);



static unsigned int StringToWord(const char *string)
{//=================================================
// Pack 4 characters into a word
	int  ix;
	unsigned char c;
	unsigned int word;

	word = 0;
	for(ix=0; ix<4; ix++)
	{
		if(string[ix]==0) break;
		c = string[ix];
		word |= (c << (ix*8));
	}
	return(word);
}



static FILE *fopen_log(FILE *f_log, const char *fname,const char *access)
{//==================================================
// performs fopen, but produces error message to f_log if it fails
	FILE *f;

	if((f = fopen(fname,access)) == NULL)
	{
		if(f_log != NULL)
			fprintf(f_log,"Can't access (%s) file '%s'\n",access,fname);
	}
	return(f);
}


static int Hash8(const char *string)
//==================================
/* Generate a hash code from the specified string */
{
   int  c;
	int  chars=0;
   int  hash=0;

   while((c = *string++) != 0)
   {
      c = tolower(c) - 'a';
      hash = hash * 8 + c;
      hash = (hash & 0x1ff) ^ (hash >> 8);    /* exclusive or */
		chars++;
   }

   return((hash+chars) & 0xff);
}   //  end of Hash8





Compile::Compile()
{//===============

	linenum = 0;
}


void Compile::Error(const char *msg)
{//=================================
	fprintf(f_errors,"%3d: %s\n",linenum,msg);
	error_count++;
}

void Compile::Error(const char *msg, const char *msg2)
{//=================================
	fprintf(f_errors,"%3d: %s: '%s'\n",linenum,msg,msg2);
	error_count++;
}



const char *Compile::PhonemeMnem(int phcode)
{//==========================================
	if((phcode >= 0) && (phcode < N_PHONEME_TAB))
		return(WordToString(phoneme_tab2[phcode].mnemonic));
	else
		return("???");
}


const char *Compile::GetKeyMnem(int value)
{//=======================================
	keywtab_t *p;

	p = keywords;

	while(p->data != -1)
	{
		if(p->data == value)
			return(p->mnem);
		p++;
	}
	return("???");
}


void Compile::ReservePhCode(const char *string, int value)
{//=======================================================
// Use a fixed number for this phoneme. Must be used before any
// phonemes are declared
	unsigned int word;

	word = StringToWord(string);

	phoneme_tab2[value].mnemonic = word;
	phoneme_tab2[value].code = value;
	if(n_phcodes <= value)
		n_phcodes = value+1;
}


int Compile::LookupPhoneme(const char *string, int control)
{//========================================================
// control = 0   explicit declaration
// control = 1   declare phoneme if not found
// control = 2   start looking after control & stress phonemes

	int  ix;
	int  start;
	int  use;
	unsigned int word;

	if(strcmp(string,"NULL")==0)
		return(1);

	ix = strlen(string);
	if((ix==0) || (ix> 4))
	{
		Error("Bad phoneme",string);
	}
	word = StringToWord(string);

	// don't use phoneme number 0, reserved for string terminator
	start = 1;
	
	if(control==2)
		start = 8;   // don't look for control and stress phonemes (allows these characters to be
		             // used for other purposes)

	use = 0;
	for(ix=start; ix<n_phcodes; ix++)
	{
		if(phoneme_tab2[ix].mnemonic == word)
			return(ix);

		if((use==0) && (phoneme_tab2[ix].mnemonic == 0))
		{
			use = ix;
		}
	}

//	if(control != 1) return(-1);  // not found

	if(use == 0)
	{
		if(n_phcodes >= N_PHONEME_TAB-1)
			return(-1);   // phoneme table is full
		use = n_phcodes++;
	}

	// add this phoneme to the phoneme table
	phoneme_tab2[use].mnemonic = word;
	phoneme_tab2[use].type = phINVALID;
	phoneme_tab2[use].spect = linenum;  // for error report if the phoneme remains undeclared
	return(use);
}  //  end of Compile::LookupPhoneme



int Compile::NextItem(int type)
{//============================
	int  acc;
	unsigned char  c=0;
	unsigned char c2;
	int  ix;
	int  sign;
	int  keyword;
	keywtab_t *p;
	
	f_in_displ = ftell(f_in);
	f_in_linenum = linenum;
		
	while(!feof(f_in))
	{
		c = fgetc(f_in);
		if(c=='/')
		{
			if((c2 = fgetc(f_in))=='/')
			{
				// comment, ignore to end of line
				while(!feof(f_in) && ((c=fgetc(f_in)) != '\n'));
			}
			else
			{
				ungetc(c2,f_in);
			}
		}
		if(c == '\n') linenum++;
		if(!isspace(c))
		{
			break;
		}
	}
	if(feof(f_in))
		return(tEND);

	if((type == tNUMBER) || (type == tSIGNEDNUMBER))
	{
		acc = 0;
		sign = 1;
		if((c == '-') && (type == tSIGNEDNUMBER))
		{
			sign = -1;
			c = fgetc(f_in);
		}
		if(!isdigit(c))
			Error("Expected number");
		while(isdigit(c))
		{
			acc *= 10;
			acc += (c - '0');
			c = fgetc(f_in);
		}
		ungetc(c,f_in);
		return(acc * sign);
	}

	ix = 0;
	while(!feof(f_in) && !isspace(c))
	{
		item_string[ix++] = c;
		c = fgetc(f_in);
		if(feof(f_in))
			break;
		if(item_string[ix-1] == '=')
			break;
	}
	item_string[ix] = 0;

	if(!feof(f_in))
		ungetc(c,f_in);

	keyword = -1;

	if(type == tSTRING)
	{
		return(0);
	}
	if(type == tKEYWORD)
	{
		p = keywords;
		while(p->data != -1)
		{
			if(strcmp(item_string,p->mnem)==0)
			{
				item_type = p->data >> 24;
				return(p->data & 0xffffff);
			}
			p++;
		}
		item_type = -1;
		return(-1);   // keyword not found
	}
	if(type == tPHONEMEMNEM)
	{
		return(LookupPhoneme(item_string,2));
	}
	return(-1);
}  //  end of Compile::NextItem


void Compile::UngetItem()
{//======================
	fseek(f_in,f_in_displ,SEEK_SET);
	linenum = f_in_linenum;
}  //  end of Compile::UngetItem



int Compile::LoadSpect(const char *path, int control)
{//=================================================
	SpectSeq *spectseq;
	int peak;
	int displ;
	int frame;
	int n_frames;
	int ix;
	int x, x2;
	int rms;
	float total;
	float pkheight;
	int marker1_set=0;
	int frame_vowelbreak=0;
	int klatt_flag=0;
	SpectFrame *fr;
	frame_t *fr_out;
	wxString path_sep = _T("/");

	SPECT_SEQ seq_out;
	SPECT_SEQK seqk_out;

	// create SpectSeq and import data
	spectseq = new SpectSeq;
	if(spectseq == NULL)
	{
		Error("Failed to create SpectSeq");
		return(0);
	}

	wxString filename = path_phsource + path_sep + wxString(path,wxConvLocal);
	wxFileInputStream stream(filename);
	
	if(stream.Ok() == FALSE)
	{
		Error("Failed to open",path);
		return(0);
	}
	spectseq->Load(stream);

	if(spectseq->synthesizer_type == 1)
		klatt_flag = FRFLAG_KLATT;

	displ = ftell(f_phdata);

	seq_out.n_frames=0;
	seq_out.flags=0;
	seq_out.length_total=0;

	total = 0;	
	for(frame=0; frame < spectseq->numframes; frame++)
	{

#ifdef deleted
for(ix=0; ix<8; ix++)
{
	// find which files have certain markers set
	if(spectseq->frames[frame]->markers & (1<<ix))
	{
		markers_used[ix]++;
		if((ix==3) || (ix==4))
		{
			fprintf(f_errors,"Marker %d: %s\n",ix,path);
		}
	}
}
#endif

		if(spectseq->frames[frame]->keyframe)
		{
			if(seq_out.n_frames == 1)
			{
				frame_vowelbreak = frame;
			}
			if(spectseq->frames[frame]->markers & 0x2)
			{
				// marker 1 is set
				marker1_set = 1;
			}

			seq_out.n_frames++;
			if(frame > 0)
				total += spectseq->frames[frame-1]->length;
		}	
	}
	seq_out.length_total = int(total);

	if((control & 1) && (marker1_set == 0))
	{
		// This is a vowel, but no Vowel Break marker is set
		// set a marker flag for the second frame of a vowel
		spectseq->frames[frame_vowelbreak]->markers |= FRFLAG_VOWEL_CENTRE;
	}

	n_frames = 0;
	for(frame=0; frame < spectseq->numframes; frame++)
	{
		fr = spectseq->frames[frame];
		
		if(fr->keyframe)
		{
			if(klatt_flag)
				fr_out = &seqk_out.frame[n_frames];
			else
				fr_out = (frame_t *)&seq_out.frame[n_frames];

			x = int(fr->length + 0.5);  // round to nearest mS
			if(x > 255) x = 255;
			fr_out->length = x;

			fr_out->frflags = fr->markers | klatt_flag;

			rms = int(fr->GetRms(spectseq->amplitude));
			if(rms > 255) rms = 255;
			fr_out->rms = rms;

			if(n_frames == (seq_out.n_frames-1))
				fr_out->length = 0;    // give last frame zero length

			// write: peak data
			count_frames++;
			for(peak=0; peak<N_PEAKS; peak++)
			{
				fr_out->ffreq[peak] = fr->peaks[peak].pkfreq;

				if(klatt_flag)
				{
					pkheight = fr->peaks[peak].pkheight / 128;
				}
				else
				{
					pkheight = spectseq->amplitude * fr->amp_adjust * fr->peaks[peak].pkheight;
					pkheight = pkheight/640000;
					if(pkheight > 255) pkheight = 255;
				}
				fr_out->fheight[peak] = int(pkheight);

				if(peak < 6)
				{
					x =  fr->peaks[peak].pkwidth/2;
					x2 =  fr->peaks[peak].pkright;
					if(klatt_flag == 0)
					{
						x /= 2;
						x2 /= 4;
					}
					if(x > 255) x = 255;
					fr_out->fwidth[peak] = x;

					if(x2 > 255) x2 = 255;
					fr_out->fright[peak] = x2;
				}
			}

			if(klatt_flag)
			{
				if(fr->klatt_param[KLATT_Kopen] == 0)
				{
					Error("Klatt_Kopen has value 0");
				}

				// additional fields
				for(ix=0; ix<N_KLATTP; ix++)
				{
					fr_out->klattp[ix] = fr->klatt_param[ix];
				}
				fr_out->fwidth6 = fr->peaks[6].pkwidth/2;
				fr_out->fright6 = fr->peaks[6].pkright;
			}

			n_frames++;
		}
	}
	
	if(klatt_flag)
	{
		seqk_out.n_frames = seq_out.n_frames;
		seqk_out.flags = seq_out.flags;
		seqk_out.length_total = seq_out.length_total;

		ix = (char *)(&seqk_out.frame[seqk_out.n_frames]) - (char *)(&seqk_out);
		ix = (ix+3) & 0xfffc;   // round up to multiple of 4 bytes
		fwrite(&seqk_out,ix,1,f_phdata);
	}
	else
	{
		ix = (char *)(&seq_out.frame[seq_out.n_frames]) - (char *)(&seq_out);
		ix = (ix+3) & 0xfffc;   // round up to multiple of 4 bytes
		fwrite(&seq_out,ix,1,f_phdata);
	}

	delete spectseq;
	return(displ);
}  //  end of Compile::LoadSpect



int Compile::LoadWavefile(FILE *f, const char *fname)
{//==================================================
	int displ;
	unsigned char c1;
	unsigned char c3;
	int c2;
	int sample;
	int sample2;
	float x;
	int max = 0;
	int length;
	int sr1, sr2;
	int resample_wav = 0;
	char fname_temp[100];
	int scale_factor=0;

	fseek(f,24,SEEK_SET);
	sr1 = Read4Bytes(f);
	sr2 = Read4Bytes(f);
	fseek(f,40,SEEK_SET);	

	if((sr1 != samplerate) || (sr2 != sr1*2))
	{
#ifdef PLATFORM_WINDOWS
		if(sr1 != samplerate)
		{
			fprintf(f_errors,"Wrong samplerate %d, wants %d\n",sr1,samplerate);
			Error("Wrong samplerate: ",fname);
		}
		if(sr2 != sr1*2)
		{
			Error("Not mono: ",fname);
		}
#else
		{
			int fd_temp;
			char command[sizeof(path_source)+200];
			strcpy(fname_temp,"/tmp/espeakXXXXXX");
			if((fd_temp = mkstemp(fname_temp)) >= 0)
			{
				close(fd_temp);
				sprintf(command,"sox \"%s%s.wav\" -r %d -c 1 -w  %s polyphase\n",path_source,fname,samplerate,fname_temp);
				if(system(command) < 0)
				{
					Error("Failed to resample: ",command);
					return(0);
				}
			}
	
			f = fopen(fname_temp,"rb");
			if(f == NULL)
			{
				Error("Can't read temp file: ",fname_temp);
				return(0);
			}
			resample_wav = 1;
			fseek(f,40,SEEK_SET);  // skip past the WAV header, up to before "data length"
		}
#endif
	}

	displ = ftell(f_phdata);

	// data contains:  4 bytes of length (n_samples * 2), followed by 2-byte samples (lsb byte first)
	length = Read4Bytes(f);

	while(!feof(f))
	{
		c1 = fgetc(f);
		c3 = fgetc(f);
		if(feof(f)) break;

		c2 = c3 << 24;
		c2 = c2 >> 16;  // sign extend

		sample = (c1 & 0xff) + c2;;

		if(sample > max)
			max = sample;
		else
		if(sample < -max)
			max = -sample;

	}

	scale_factor = (max / 127) + 1;

//fprintf(f_errors," sample len=%d max=%4x shift=%d\n",length,max,scale_factor);

#define MIN_FACTOR   -1  // was 6,  disable use of 16 bit samples
	if(scale_factor > MIN_FACTOR)
	{
		length = length/2 + (scale_factor << 16);
	}

	Write4Bytes(f_phdata,length);
//	fwrite(&length,4,1,f_phdata);
	fseek(f,44,SEEK_SET);

	while(!feof(f))
	{
		c1 = fgetc(f);
		c3 = fgetc(f);
		c2 = c3 << 24;
		c2 = c2 >> 16;  // sign extend

		sample = (c1 & 0xff) + c2;

		if(feof(f)) break;

      if(scale_factor <= MIN_FACTOR)
		{
			fputc(sample & 0xff,f_phdata);
			fputc(sample >> 8,f_phdata);
		}
		else
		{
			x = (float(sample) / scale_factor) + 0.5;
			sample2= int(x);
			if(sample2 > 127)
				sample2 = 127;
			if(sample2 < -128)
				sample2 = -128;
			fputc(sample2,f_phdata);
		}
	}

	length = ftell(f_phdata);
	while((length & 3) != 0)
	{
		// pad to a multiple of 4 bytes
		fputc(0,f_phdata);
		length++;
	}

	if(resample_wav != 0)
	{
		fclose(f);
		remove(fname_temp);
	}
	return(displ | 0x800000);  // set bit 23 to indicate a wave file rather than a spectrum
}  //  end of Compile::LoadWavefile



int Compile::LoadEnvelope(FILE *f, const char *fname)
{//==================================================
	int displ;
	char buf[128];

	displ = ftell(f_phdata);

	fseek(f,12,SEEK_SET);
	fread(buf,1,128,f);
	fwrite(buf,1,128,f_phdata);

	if(n_envelopes < N_ENVELOPES)
	{
		strncpy0(envelope_paths[n_envelopes],fname,sizeof(envelope_paths[0]));
		memcpy(envelope_dat[n_envelopes],buf,sizeof(envelope_dat[0]));
		n_envelopes++;
	}

	return(displ);
}


int Compile::LoadDataFile(const char *path, int control)
{//====================================================
	// load spectrum sequence or sample data from a file.
	// return index into spect or sample data area. bit 23=1 if a sample

	FILE *f;
	int id;
	int hash;
	int addr = 0;
	int type_code=' ';
	REF_HASH_TAB *p, *p2;
	char buf[sizeof(path_source)+120];

	if(strcmp(path,"NULL")==0)
		return(0);
	if(strcmp(path,"DFT")==0)
		return(1);

	count_references++;

	hash = Hash8(path);
	p = ref_hash_tab[hash];
	while(p != NULL)
	{
		if(strcmp(path,p->string)==0)
		{
			duplicate_references++;
			addr = p->value;    // already loaded this data
			break;
		}
		p = (REF_HASH_TAB *)p->link;
	}

	if(addr == 0)
	{
		sprintf(buf,"%s%s",path_source,path);
	
		if((f = fopen(buf,"rb")) == NULL)
		{
			sprintf(buf,"%s%s.wav",path_source,path);
			if((f = fopen(buf,"rb")) == NULL)
			{
				Error("Can't read file",path);
				return(0);
			}
		}

		id = Read4Bytes(f);
		rewind(f);
	
		if(id == 0x43455053)
		{
			addr = LoadSpect(path, control);
			type_code = 'S';
		}
		else
		if(id == 0x46464952)
		{
			addr = LoadWavefile(f,path);
			type_code = 'W';
		}
		else
		if(id == 0x43544950)
		{
			addr = LoadEnvelope(f,path);
			type_code = 'E';
		}
		else
		if(id == 0x45564E45)
		{
			addr = LoadEnvelope2(f,path);
			type_code = 'E';
		}
		else
		{
			Error("File not SPEC or RIFF",path);
			addr = -1;
		}
		fclose(f);
	
		if(addr > 0)
		{
			fprintf(f_phcontents,"%c  0x%.5x  %s\n",type_code,addr & 0x7fffff,path);
		}
	}

	// add this item to the hash table
	if(addr > 0)
	{
		p = ref_hash_tab[hash];
		p2 = (REF_HASH_TAB *)malloc(sizeof(REF_HASH_TAB)+strlen(path)+1);
		p2->value = addr;
		p2->ph_mnemonic = ph->mnemonic;   // phoneme which uses this file
		p2->ph_table = n_phoneme_tabs-1;
		strcpy(p2->string,path);
		p2->link = (char *)p;
		ref_hash_tab[hash] = p2;
	}

	return(addr);
}   //  end of Compile::LoadDataFile



void Compile::AddEnvelope(int *list)
{//================================
	NextItem(tSTRING);
	*list++ = LoadDataFile(item_string,0);
	list[0] = 0;
}


int Compile::AddSpect(int phcode, int *list, int control)
{//=====================================================
	int sign;
	int sign2;
	int all_digits;
	char *p;
	char *p2;
	int spect;
	int value;
	int count = 0;
	int v_in = vowel_in[0];
	int v_out = vowel_out[0];

	sign = 0;
	all_digits=0;
	p = p2 = item_string;
	for(;;)
	{
		if((*p == 0) || (*p == '+') || (((*p == '-') || (*p == '%')) && isdigit(p[1])))
		{
			sign2 = *p;
			*p = 0;
			
			if(all_digits)
			{
				value = atoi(p2);
				if(sign == '+')
					*list++ = 1 + (value<<8);
				else
				if(sign == '-')
					*list++ = 2 + (value<<8);
				else
				{
					value = (value * 32)/100;  // convert from % to 1/32s
					*list++ = 3 + (value<<8);
				}
				count++;
			}
			else
			{
				spect = LoadDataFile(p2, control);
				if(spect != -1)
				{
					*list++ = phcode + (spect<<8);
					count++;


					// vowel formant transitions specified ?
					if(v_in != 0)
					{
						*list++ = 0x4 + (vowel_in[0] << 8);
						*list++ = vowel_in[1];
						v_in = 0;
						count+=2;
					}
					if(v_out != 0)
					{
						*list++ = 0x5 + (vowel_out[0] << 8);
						*list++ = vowel_out[1];
						v_out = 0;
						count+=2;
					}

				}
				phcode = 0;
			}
			
			if((sign = sign2)==0) break;
			p2 = p+1;
			all_digits=1;
		}
		else
		{
			if(!isdigit(*p))
				all_digits=0;
		}
		p++;
	}
	*list = 0;
	return(count);
}  // end of  AddSpect



void Compile::AddSpectList(int *list, int control)
{//==============================================
	int phcode;
	int key;

	while(*list != 0) list++;   // find end of the list
	
	for(;;)
	{
		key = NextItem(tKEYWORD);
		UngetItem();
		if(key != -1)
			break;
		
		phcode = NextItem(tPHONEMEMNEM);
		
		if(phcode == -1)
			Error("Undeclared phoneme",item_string);

		if(NextItem(tSTRING) == -1)
			break;

		list += AddSpect(phcode, list, control);
	}

	*list++ = 0;
}  //  end of Compile::AddSpectList



static int Range(int value, int divide, int min, int max)
{//======================================================
	if(value < 0)
		value -= divide/2;
	else
		value += divide/2;
	value = value / divide;

	if(value > max)
		value = max;
	if(value < min)
		value = min;
	return(value - min);
}


void Compile::VowelTransition(int which, unsigned int *trans)
{//==========================================================
// Compile a vowel transition
	int key;
	int len=0;
	int rms=0;
	int f1=0;
	int f2=0;
	int f2_min=0;
	int f2_max=0;
	int f3_adj=0;
	int f3_amp=0;
	int flags=0;
	int vcolour=0;
	int x;

	if(which==1)
	{
		len = 50 / 2;  // defaults for transition into vowel
		rms = 25 / 2;

		if(ph->type == phSTOP)
		{
			len = 42 / 2;  // defaults for transition into vowel
			rms = 30 / 2;
		}
	}
	else
	if(which==2)
	{
		len = 36 / 2;  // defaults for transition out of vowel
		rms = 16 / 2;
	}

	for(;;)
	{
		key = NextItem(tKEYWORD);
		if(item_type != 3)
		{
			UngetItem();
			break;
		}

		switch(key & 0xf)
		{
		case 1:
			len = Range(NextItem(tNUMBER), 2, 0, 63) & 0x3f;
			flags |= 1;
			break;
		case 2:
			rms = Range(NextItem(tNUMBER), 2, 0, 31) & 0x1f;
			flags |= 1;
			break;
		case 3:
			f1 = NextItem(tNUMBER);
			break;
		case 4:
			f2 = Range(NextItem(tNUMBER), 50, 0, 63) & 0x3f;
			f2_min = Range(NextItem(tSIGNEDNUMBER), 50, -15, 15) & 0x1f;
			f2_max = Range(NextItem(tSIGNEDNUMBER), 50, -15, 15) & 0x1f;
			if(f2_min > f2_max)
			{
				x = f2_min;
				f2_min = f2_max;
				f2_max = x;
			}
			break;
		case 5:
			f3_adj = Range(NextItem(tSIGNEDNUMBER), 50, -15, 15) & 0x1f;
			f3_amp = Range(NextItem(tNUMBER), 8, 0, 15) & 0x1f;
			break;
		case 6:
			flags |= 2;   // break
			break;
		case 7:
			flags |= 4;   // rate
			break;
		case 8:
			flags |= 8;   // glstop
			break;
		case 9:
			flags |= 16;  // lenadd
			break;
		case 10:
			flags |= 32;  // f4
			break;
		case 11:
			flags |= 64;  // paus
			break;
		case 12:
			vcolour = NextItem(tNUMBER);
			break;
		case 13:
			// set rms of 1st frame as fraction of rms of 2nd frame  (1/30ths)
			rms = (Range(NextItem(tNUMBER), 1, 0, 31) & 0x1f) | 0x20;
			flags |= 1;
			break;
		}
	}
	trans[0] = len + (rms << 6) + (flags << 12) + 0x80000000;
	trans[1] =  f2 + (f2_min << 6) + (f2_max << 11) + (f3_adj << 16) + (f3_amp << 21) + (f1 << 26) + (vcolour << 29);
}  // end of VowelTransition


void Compile::ImportPhoneme(void)
{//==============================
	int ix;
	unsigned int mnem;
	unsigned int ph_mnem;
	unsigned int ph_code;
	PHONEME_TAB_LIST *phtab = NULL;

	NextItem(tSTRING);
	mnem = StringToWord(item_string);

	NextItem(tSTRING);
	for(ix=0; ix<n_phoneme_tabs; ix++)
	{
		if(strcmp(phoneme_tab_list2[ix].name,item_string) == 0)
		{
			phtab = &phoneme_tab_list2[ix];
			break;
		}
	}
	if(phtab == NULL)
	{
		Error("Unknown phoneme table",item_string);
		return;  // phoneme table not found
	}

	for(ix=1; ix<256; ix++)
	{
		if(mnem == phtab->phoneme_tab_ptr[ix].mnemonic)
		{
			ph_mnem = ph->mnemonic;
			ph_code = ph->code;
			memcpy(ph,&phtab->phoneme_tab_ptr[ix],sizeof(PHONEME_TAB));
			ph->mnemonic = ph_mnem;
			ph->code = ph_code;
			break;
		}
	}
	if(ix == 256)
	{
		Error("Import phoneme not found",WordToString(mnem));
	}
}

int Compile::CPhoneme()
{//====================
	int phcode;
	int phindex;
	int value;
	int item;
	int finish=0;
	int *intp;
	int before_tab[N_PHONEME_TAB];
	int after_tab[N_PHONEME_TAB];
	int default_spect[N_PHONEME_TAB];

	NextItem(tSTRING);
	phindex = LookupPhoneme(item_string,1);    // declare phoneme if not already there
	if(phindex == -1) return(0);

	ph = &phoneme_tab2[phindex];
	memset(ph,0,sizeof(PHONEME_TAB));
	ph->mnemonic = StringToWord(item_string);
	ph->type = 0xff;   // undefined
	ph->code = phindex;
	before_tab[0] = 0;
	after_tab[0] = 0;
	default_spect[0] = 0;
	vowel_in[0] = 0;
	vowel_in[1] = 0;
	vowel_out[0] = 0;
	vowel_out[1] = 0;

	while(!finish)
	{
		item = NextItem(tKEYWORD);

		if(item_type==2)
			ph->phflags |= item & 0xffffff;
		else
		if(item_type==1)
		{
			if(ph->type != 0xff)
			{
				Error("Phoneme type already set",item_string);
			}
			ph->type = item & 0xff;
		}
		else
		switch(item)
		{
		case tLENGTHMOD:
			ph->length_mod = NextItem(tNUMBER);
			break;

		case tLENGTH:
			ph->std_length = NextItem(tNUMBER);
			break;

		case tSTRESSTYPE:
			ph->std_length = NextItem(tNUMBER);
			break;

		case tWAVE:
			ph->phflags |= phWAVE;           // drop through to tSPECT
		case tSPECT:
			if(NextItem(tSTRING) == -1)
				Error("Syntax error",item_string);
			else
			{
				if(ph->type == phVOWEL)
					AddSpect(phonPAUSE,default_spect,1);
				else
					AddSpect(phonPAUSE,default_spect,0);
			}
			break;

		case tSTARTTYPE:
			phcode = NextItem(tPHONEMEMNEM);
			if(phcode == -1)
				phcode = LookupPhoneme(item_string,1);
			ph->start_type = phcode;
			break;

		case tENDTYPE:
			phcode = NextItem(tPHONEMEMNEM);
			if(phcode == -1)
				phcode = LookupPhoneme(item_string,1);
			ph->end_type = phcode;
			break;

		case tTONESPEC:
			ph->start_type = NextItem(tNUMBER);   // tone's min pitch (range 0-50)
			ph->end_type = NextItem(tNUMBER);     // tone's max pitch (range 0-50)
			if(ph->start_type > ph->end_type)
			{
				// ensure pitch1 < pitch2
				value = ph->start_type;
				ph->start_type = ph->end_type;
				ph->end_type = value;
			}
			AddEnvelope(default_spect);       // envelope for pitch change,  rise or fall or fall/rise, etc
			AddEnvelope(after_tab);           // envelope for amplitude change
			break;
			
		case tREDUCETO:
			phcode = NextItem(tPHONEMEMNEM);
			if(phcode == -1)
				phcode = LookupPhoneme(item_string,1);
			ph->reduce_to = phcode;
			value = NextItem(tNUMBER);
			ph->phflags = (ph->phflags & 0xfffffff) + (value << 28);
			break;

		case tBEFOREVOWEL:
		case tBEFOREVOWELPAUSE:
		case tBEFORENOTVOWEL:
		case tBEFORENOTVOWEL2:
		case tSWITCHVOICING:
		case tBEFORE_R:
			ph->phflags |= flags_alternative[item - tBEFOREVOWEL];
			if((phcode = NextItem(tPHONEMEMNEM)) == -1)
				phcode = LookupPhoneme(item_string,1);
			ph->alternative_ph = phcode;
			break;

		case tBEFOREPAUSE:
			ph->phflags |= phBEFOREPAUSE;
			if((phcode = NextItem(tPHONEMEMNEM)) == -1)
				phcode = LookupPhoneme(item_string,1);
			ph->link_out = phcode;
			break;

		case tAPPENDPH:
			ph->phflags |= phAPPENDPH;  // drop through to tLINKOUT
		case tLINKOUT:
			phcode = NextItem(tPHONEMEMNEM);
			if(phcode == -1)
				phcode = LookupPhoneme(item_string,1);
			ph->link_out = phcode;
			break;

		case tBEFORE:
			AddSpectList(before_tab,0);
			break;
			
		case tAFTER:
			AddSpectList(after_tab,0);
			break;

		case tEND:
			finish = 1;
			break;

		case tVOWELIN:
			VowelTransition(1,vowel_in);
			break;

		case tVOWELOUT:
			VowelTransition(2,vowel_out);
			break;

		case tIMPORTPH:
			ImportPhoneme();
			break;

		default:
			Error("Syntax error",item_string);
			break;
		}
	}
	// write out indices
	if(before_tab[0] != 0)
	{
		ph->before = ftell(f_phindex) / 4;
		intp = before_tab;
		for(;;)
		{
			fwrite(intp,4,1,f_phindex);
			if(*intp++ == 0) break;
		}
	}
	if(after_tab[0] != 0)
	{
		ph->after = ftell(f_phindex) / 4;
		intp = after_tab;
		for(;;)
		{
			fwrite(intp,4,1,f_phindex);
			if(*intp++ == 0) break;
		}
	}
	if(default_spect[0] != 0)
	{
		ph->spect = ftell(f_phindex) / 4;
		intp = default_spect;
		for(;;)
		{
			fwrite(intp,4,1,f_phindex);
			if(*intp++ == 0) break;
		}
	}

	if(ph->phflags & phVOICED)
	{
		if(ph->type == phSTOP)
			ph->type = phVSTOP;
		else
		if(ph->type == phFRICATIVE)
			ph->type = phVFRICATIVE;
	}

	ph->std_length |= 0x8000;  // 'locally declared' indicator

	if(ph->type == phDELETED)
	{
		ph->mnemonic = 0x01;  // will not be recognised
	}

	return(phindex);
}  // end of Compile::CPhoneme



void Compile::WritePhonemeTable()
{//==============================
	int ix;
	int j;
	int n;
	int value;
	int count;
	PHONEME_TAB *p;

	value = n_phoneme_tabs;
	fputc(value,f_phtab);
	fputc(0,f_phtab);
	fputc(0,f_phtab);
	fputc(0,f_phtab);

	for(ix=0; ix<n_phoneme_tabs; ix++)
	{
		p = phoneme_tab_list2[ix].phoneme_tab_ptr;
		n = n_phcodes_list[ix];
		p[n].mnemonic = 0;   // terminate the phoneme table

		// count number of locally declared phonemes
		count=0;
		for(j=0; j<n; j++)
		{
			if(ix==0)
				p[j].std_length |= 0x8000;  // write all phonemes in the base phoneme table

			if(p[j].std_length & 0x8000)
				count++;
		}
		phoneme_tab_list2[ix].n_phonemes = count+1;

		fputc(count+1,f_phtab);
		fputc(phoneme_tab_list2[ix].includes,f_phtab);
		fputc(0,f_phtab);
		fputc(0,f_phtab);

		fwrite(phoneme_tab_list2[ix].name,1,N_PHONEME_TAB_NAME,f_phtab);

		for(j=0; j<n; j++)
		{
			if(p[j].std_length & 0x8000)
			{
				// this bit is set temporarily to incidate a local phoneme, declared in
				// in the current phoneme file
				p[j].std_length &= 0x7fff;
				fwrite(&p[j],sizeof(PHONEME_TAB),1,f_phtab);
			}
		}
		fwrite(&p[n],sizeof(PHONEME_TAB),1,f_phtab);  // include the extra list-terminator phoneme entry
		free(p);
	}
}


void Compile::EndPhonemeTable()
{//============================
	int  ix;

	if(n_phoneme_tabs == 0)
		return;

	// check that all referenced phonemes have been declared
	for(ix=0; ix<n_phcodes; ix++)
	{
		if(phoneme_tab2[ix].type == phINVALID)
		{
			fprintf(f_errors,"%3d: Phoneme [%s] not declared, referenced at line %d\n",linenum,
				WordToString(phoneme_tab2[ix].mnemonic),int(phoneme_tab2[ix].spect));
			error_count++;
		}
	}

	n_phcodes_list[n_phoneme_tabs-1] = n_phcodes;
}


void Compile::StartPhonemeTable(const char *name)
{//==============================================
	int ix;
	int j;
	PHONEME_TAB *p;

	fprintf(f_errors,"______________________________\nPhoneme Table: '%s'\n",name);

	if(n_phoneme_tabs >= N_PHONEME_TABS-1)
	{
		Error("Too many phonemetables");
		return;
	}
	p = (PHONEME_TAB *)calloc(sizeof(PHONEME_TAB),N_PHONEME_TAB);

	if(p == NULL)
	{
		Error("Out of memory");
		return;
	}

	if(gui_flag)
		progress->Update(n_phoneme_tabs);

	phoneme_tab_list2[n_phoneme_tabs].phoneme_tab_ptr = phoneme_tab2 = p;
	memset(phoneme_tab_list2[n_phoneme_tabs].name, 0, N_PHONEME_TAB_NAME);
	strncpy0(phoneme_tab_list2[n_phoneme_tabs].name, name, N_PHONEME_TAB_NAME);
	n_phcodes = 1;
	phoneme_tab_list2[n_phoneme_tabs].includes = 0;

	if(n_phoneme_tabs > 0)
	{
		NextItem(tSTRING);	// name of base phoneme table
		for(ix=0; ix<n_phoneme_tabs; ix++)
		{
			if(strcmp(item_string,phoneme_tab_list2[ix].name)==0)
			{
				phoneme_tab_list2[n_phoneme_tabs].includes = ix+1;

				// initialise the new phoneme table with the contents of this one
				memcpy(phoneme_tab2,phoneme_tab_list2[ix].phoneme_tab_ptr,sizeof(PHONEME_TAB)*N_PHONEME_TAB);
				n_phcodes = n_phcodes_list[ix];

				// clear "local phoneme" bit"
				for(j=0; j<n_phcodes; j++)
					phoneme_tab2[j].std_length = phoneme_tab2[j].std_length & 0x7fff;
				break;
			}
		}
		if(ix == n_phoneme_tabs)
		{
			Error("Can't find base phonemetable '%s'",item_string);
		}
	}

	n_phoneme_tabs++;
}  // end of StartPhonemeTable



void Compile::CPhonemeFiles()
{//==========================
	int phcode;
	int item;
	FILE *f;
	char buf[sizeof(path_source)+120];

	linenum = 1;
	n_phcodes = 1;  // don't use phoneme code=0, it's used as string terminator

	count_references = 0;
	duplicate_references = 0;
	count_frames = 0;

	for(;;)
	{
		if(feof(f_in))
		{
			// end of file, go back to previous from, from which this was included

			if(stack_ix == 0)
				break;   // end of top level, finished
			fclose(f_in);
			f_in = stack[--stack_ix].file;
			strcpy(current_fname,stack[stack_ix].fname);
			linenum = stack[stack_ix].linenum;
			fprintf(f_errors,"\n\n");
		}

		item = NextItem(tKEYWORD);

		if(item == tINCLUDE)
		{
			NextItem(tSTRING);
			sprintf(buf,"%s%s",path_source,item_string);
			if((stack_ix < N_STACK) && (f = fopen_log(f_errors,buf,"rb")) != NULL)
			{
				fprintf(f_errors,"include %s\n",item_string);
				stack[stack_ix].linenum = linenum;
				strcpy(stack[stack_ix].fname,current_fname);
				stack[stack_ix++].file = f_in;
				
				f_in = f;
				strncpy0(current_fname,item_string,sizeof(current_fname));
				linenum = 1;
			}
		}
		else
		if(item == tPHONEMETABLE)
		{
			EndPhonemeTable();
			NextItem(tSTRING);	// name of the new phoneme table
			StartPhonemeTable(item_string);
		}
		else
		if(item == tPHONEMENUMBER)
		{
			// reserve a specified number for this phoneme
			phcode = NextItem(tNUMBER);
			NextItem(tSTRING);
			ReservePhCode(item_string,phcode);
		}
		else
		if(item == tPHONEMESTART)
		{
			if(n_phoneme_tabs == 0)
			{
				Error("phonemetable is missing");
				return;
			}
			phcode = CPhoneme();
		}
		else
		if(!feof(f_in))
			Error("Keyword 'phoneme' expected");
	}
	phoneme_tab2[n_phcodes+1].mnemonic = 0;  // terminator
}  //  end of CPhonemeFiles



int LoadEnvelope2(FILE *f, const char *fname)
{//===================================================
	int ix, ix2;
	int n;
	int x, y;
	int displ;
	int n_points;
	double yy;
	char line_buf[128];
	float env_x[20];
	float env_y[20];
	int env_lin[20];
	unsigned char env[ENV_LEN];

	n_points = 0;
	fgets(line_buf,sizeof(line_buf),f);   // skip first line
	while(!feof(f))
	{
		if(fgets(line_buf,sizeof(line_buf),f) == NULL)
			break;

		env_lin[n_points] = 0;
		n = sscanf(line_buf,"%f %f %d",&env_x[n_points],&env_y[n_points],&env_lin[n_points]);
		if(n >= 2)
		{
			env_x[n_points] *= (float)1.28;  // convert range 0-100 to 0-128
			n_points++;
		}
	}
	env_x[n_points] = env_x[n_points-1];
	env_y[n_points] = env_y[n_points-1];

	ix = -1;
	ix2 = 0;
	for(x=0; x<ENV_LEN; x++)
	{
		if(x > env_x[ix+4])
			ix++;
		if(x >= env_x[ix2+1])
			ix2++;

		if(env_lin[ix2] > 0)
		{
			yy = env_y[ix2] + (env_y[ix2+1] - env_y[ix2]) * (float(x) - env_x[ix2]) / (env_x[ix2+1] - env_x[ix2]);
			y = int(yy * 2.55);
		}
		else
		if(n_points > 3)
			y = (int)(polint(&env_x[ix],&env_y[ix],4,x) * 2.55);  // convert to range 0-255
		else
			y = (int)(polint(&env_x[ix],&env_y[ix],3,x) * 2.55);
		if(y < 0) y = 0;
		if(y > 255) y = 255;
		env[x] = y;
	}


	if(n_envelopes < N_ENVELOPES)
	{
		strncpy0(envelope_paths[n_envelopes],fname,sizeof(envelope_paths[0]));
		memcpy(envelope_dat[n_envelopes],env,ENV_LEN);
		n_envelopes++;
	}

	displ = ftell(f_phdata);
	fwrite(env,1,128,f_phdata);

	return(displ);
}


static int ref_sorter(char **a, char **b)
{//======================================
	int ix;

	REF_HASH_TAB *p1 = (REF_HASH_TAB *)(*a);
	REF_HASH_TAB *p2 = (REF_HASH_TAB *)(*b);

	ix = strcoll(p1->string,p2->string);
	if(ix != 0)
		return ix;

	ix = p1->ph_table - p2->ph_table;
	if(ix != 0)
		return ix;

	return(p1->ph_mnemonic - p2->ph_mnemonic);
}   /* end of ref_sorter */



void Compile::Report(void)
{//=======================
	int ix;
	int hash;
	int n;
	REF_HASH_TAB *p;
	REF_HASH_TAB **list;
	FILE *f_report;
	const char *data_path;
	int prev_table;
	int prev_mnemonic;
	char fname[sizeof(path_source)+20];

	// make a list of all the references and sort it
	list = (REF_HASH_TAB **)malloc((count_references)* sizeof(REF_HASH_TAB *));
	if(list == NULL)
		return;

	sprintf(fname,"%scompile_report",path_source);
	f_report = fopen(fname,"w");
	if(f_report == NULL)
	{
		free(list);
		return;
	}

	fprintf(f_report,"%d phoneme tables\n",n_phoneme_tabs);
   fprintf(f_report,"          new total\n");
	for(ix=0; ix<n_phoneme_tabs; ix++)
	{
		fprintf(f_report,"%8s %3d %4d\n",phoneme_tab_list2[ix].name, phoneme_tab_list2[ix].n_phonemes, n_phcodes_list[ix]+1);
	}
	fputc('\n',f_report);

	fprintf(f_report,"Data file      Used by\n");
	ix = 0;
	for(hash=0; (hash < 256) && (ix < count_references); hash++)
	{
		p = ref_hash_tab[hash];
		while(p != NULL)
		{
			list[ix++] = p;
			p = (REF_HASH_TAB *)(p->link);
		}
	}
	n = ix;
	qsort((void *)list,n,sizeof(REF_HASH_TAB *),(int (*)(const void *,const void *))ref_sorter);
	
	data_path = "";
	prev_mnemonic = 0;
	prev_table = 0;
	for(ix=0; ix<n; ix++)
	{
		int j = 0;

		if(strcmp(list[ix]->string, data_path) != 0)
		{
			data_path = list[ix]->string;
			j = strlen(data_path);
			fprintf(f_report,"%s",data_path);
		}
		else
		{
			if((list[ix]->ph_table == prev_table) && (list[ix]->ph_mnemonic == prev_mnemonic))
				continue;   // same phoneme, don't list twice
		}

		while(j < 14)
		{
			fputc(' ',f_report);  // pad filename with spaces
			j++;
		}

		fprintf(f_report,"  [%s] %s",WordToString(prev_mnemonic = list[ix]->ph_mnemonic), phoneme_tab_list2[prev_table = list[ix]->ph_table].name);
		fputc('\n',f_report);
	}

	for(ix=0; ix<n; ix++)
	{
		free(list[ix]);
		list[ix] = NULL;
	}

	free(list);
	list = NULL;
	fclose(f_report);
}


#ifdef PLATFORM_WINDOWS
int strcasecmp(const char *s1, const char *s2)
{
	int ix=0;
	int diff;

	for(;;)
	{
		if((diff = (tolower(s1[ix]) - tolower(s2[ix]))) != 0)
			return(diff);

		if((s1[ix] == 0) || (s2[ix] == 0))
			return(diff);

		ix++;
	}
}
#endif


static int ph_sorter(char **a, char **b)
{//======================================
	int ix;
	int t1, t2;
	char mnem1[6];

	PHONEME_TAB *p1 = (PHONEME_TAB *)(*a);
	PHONEME_TAB *p2 = (PHONEME_TAB *)(*b);

	t1 = p1->type;
	if(t1 > phVOWEL) t1 = phVOWEL+1;

	t2 = p2->type;
	if(t2 > phVOWEL) t2 = phVOWEL+1;

	if((ix = t1 - t2) != 0)
		return(ix);

	strcpy(mnem1,WordToString(p1->mnemonic));
  return(strcasecmp(mnem1,WordToString(p2->mnemonic)));
}   /* end of ph_sorter */



void PrintPhonemesUsed(FILE *f, const char *dictname)
{//==================================================
	int ix;
	PHONEME_TAB *ph;
	PHONEME_TAB *ph_tab[N_PHONEME_TAB];
	int count = 0;
	int n_ph = 0;
	int section = 0;

	fprintf(f,"\n\nDictionary %s_dict\n",dictname);
	fflush(f);

	for(ix=0; (ix<N_PHONEME_TAB) && (phoneme_tab[ix] != NULL); ix++)
	{
		if(phoneme_tab_flags[ix] & 2)
		{
			ph_tab[n_ph++] = phoneme_tab[ix];
		}
	}

	qsort((void *)ph_tab,n_ph,sizeof(PHONEME_TAB *),(int (*)(const void *,const void *))ph_sorter);

	for(ix=0; ix<n_ph; ix++)
	{
		ph = ph_tab[ix];

		if(ph->type > 1)
		{
			if((ph->type > phVOWEL) && (section == 0))
			{
				section = 1;
				count = 0;
				fputc('\n',f);
			}
			if((count & 0x7) == 0)
				fputc('\n',f);
			fprintf(f,"%-4s ",WordToString(ph->mnemonic));
			count++;
		}
	}
	fputc('\n',f);
}  // end of  PrintPhonemesUsed



wxString CompileAllDictionaries()
{//==============================
	wxString filename;
	wxFileName fname;
	wxString dictstr;
	wxString report = _T("");
	int err;
	int errors = 0;
	int dict_count = 0;
	FILE *f_in;
	FILE *log;
	FILE *f_phused;
	char dictname[80];
	char fname_log[sizeof(path_dsource)+20];
	char save_voice_name[80];
	char path[sizeof(path_home)+40];       // path_dsource+20
	char buf[200];
	char voicename[80];

	if(!wxDirExists(path_dictsource))
	{
		if(gui_flag)
		{
			wxString dirname = wxDirSelector(_T("Directory of dictionary files"),path_phsource);
			if(!dirname.IsEmpty())
			{
				path_dictsource = dirname;
				strncpy0(path_dsource,path_dictsource.mb_str(wxConvLocal),sizeof(path_dsource)-1);
				strcat(path_dsource,"/");
			}
		}
		else
		{
			fprintf(stderr,"Can't find dictionary files: %s\n",path_dsource);
		}
	}

	wxDir dir(path_dictsource);
	if(!dir.IsOpened())
	{
		return(_T(" No dictionaries"));
	}

	strcpy(save_voice_name,voice_name2);

	sprintf(fname_log,"%s%s",path_dsource,"dict_log");
	log = fopen(fname_log,"w");
	sprintf(fname_log,"%s%s",path_dsource,"dict_phonemes");
	f_phused = fopen(fname_log,"w");

	if(f_phused)
	{
		fprintf(f_phused,"Phonemes which are used in the *_rules and *_list files\n");
	}

	bool cont = dir.GetFirst(&filename, _T("*_rules"), wxDIR_FILES);
	while ( cont )
	{
		fname = wxFileName(filename);
		dictstr = fname.GetName().BeforeLast('_');
		strcpy(dictname,dictstr.mb_str(wxConvLocal));
		dict_count++;
		strcpy(voicename,dictname);

		// read the *_rules file to see if a phoneme table is specified though a voice name
		sprintf(path,"%s%s_rules",path_dsource,dictname);
		if((f_in = fopen(path,"r")) != NULL)
		{
			unsigned int ix;
			unsigned int c;

			for(ix=0; ix<20; ix++)
			{
				if(fgets(buf,sizeof(buf),f_in) == NULL)
					break;

				if(memcmp(buf,"//voice=",8)==0)
				{
					for(ix=0; ix<sizeof(voicename); ix++)
					{
						if(isspace(c = buf[ix+8]))
						{
							break;
						}
						voicename[ix] = c;
					}
					voicename[ix] = 0;
					break;
				}
			}
			fclose(f_in);
		}

		LoadVoice(voicename,0);

		if((err = CompileDictionary(path_dsource, dictname,log,NULL,0)) > 0)
		{
			report = report + dictstr + wxString::Format(_T(" %d, "),err);
			errors += err;
		}

		if(f_phused != NULL)
		{
			memset(phoneme_tab_flags,0,sizeof(phoneme_tab_flags));
			FindPhonemesUsed();
			PrintPhonemesUsed(f_phused,dictname);
		}

		cont = dir.GetNext(&filename);
	}
	if(log != NULL)
		fclose(log);
	if(f_phused != NULL)
		fclose(f_phused);

	LoadVoice(save_voice_name,1);

	if(errors == 0)
		return(wxString::Format(_T(" Compiled %d dictionaries"),dict_count));
	else
	{
		return(_T(" Dictionary errors: ") + report);
	}

}  // end of CompileAllDictionaries



void Compile::CPhonemeTab(const char *source)
{//========================================
	char fname[sizeof(path_source)+40];
	wxString report;
	wxString report_dict;

#ifdef MAKE_ENVELOPES
make_envs();	
#endif

	n_envelopes = 0;
	error_count = 0;
memset(markers_used,0,sizeof(markers_used));

	f_errors = stderr;

	strncpy0(current_fname,source,sizeof(current_fname));

	strncpy0(fname,path_phfile.mb_str(wxConvLocal),sizeof(fname));
	f_in = fopen_log(f_errors,fname,"rb");
	if(f_in == NULL)
	{
		if(gui_flag)
		{
			wxString phfile = wxFileSelector(_T("Master phonemes file"),path_phsource,
			_T(""),_T(""),_T("*"),wxOPEN);

			if(!phfile.IsEmpty())
			{
				path_phfile = phfile;
			}
		}

		strncpy0(fname,path_phfile.mb_str(wxConvLocal),sizeof(fname));
		f_in = fopen_log(f_errors,fname,"rb");
		if(f_in == NULL)
		{
			wxLogError(_T("Can't read master phonemes file:\n") + wxString(fname,wxConvLocal));
			return;
		}
	}

	progress_max = 0;
	while(fgets(fname,sizeof(fname),f_in) != NULL)
	{
		// count the number of phoneme tables declared in the master phonemes file
		if(memcmp(fname,"phonemetable",12)==0)
			progress_max++;
	}
	rewind(f_in);

	sprintf(fname,"%s%s",path_source,"error_log");
	if((f_errors = fopen_log(f_errors,fname,"w")) == NULL)
		f_errors = stderr;

	sprintf(fname,"%s/%s",path_home,"phondata-manifest");
	if((f_phcontents = fopen_log(f_phcontents,fname,"w")) == NULL)
		f_phcontents = stderr;

	fprintf (f_phcontents,
"# This file lists the type of data that has been compiled into the\n"
"# phondata file\n"
"#\n"
"# The first character of a line indicates the type of data:\n"
"#   S - A SPECT_SEQ structure\n"
"#   W - A wavefile segment\n"
"#   E - An envelope\n"
"#\n"
"# Address is the displacement within phondata of this item\n"
"#\n"
"#  Address  Data file\n"
"#  -------  ---------\n");


	sprintf(fname,"%s/%s",path_home,"phondata");
	f_phdata = fopen_log(f_errors,fname,"wb");

	sprintf(fname,"%s/%s",path_home,"phonindex");
	f_phindex = fopen_log(f_errors,fname,"wb");

	sprintf(fname,"%s/%s",path_home,"phontab");
	f_phtab = fopen_log(f_errors,fname,"wb");

	if(f_phdata==NULL || f_phindex==NULL || f_phtab==NULL)
	{
		return;
	}

	if(gui_flag)
	{
		progress = new wxProgressDialog(_T("Phonemes"),_T(""),progress_max);
	}
	else
	{
		fprintf(stderr,"Compiling phoneme data: %s\n",path_source);
	}

	// write a word so that further data doesn't start at displ=0
	Write4Bytes(f_phdata,version_phdata);
	Write4Bytes(f_phindex,version_phdata);

	memset(ref_hash_tab,0,sizeof(ref_hash_tab));

	n_phoneme_tabs = 0;
	stack_ix = 0;
	StartPhonemeTable("base");
	CPhonemeFiles();

	EndPhonemeTable();
	WritePhonemeTable();

fprintf(f_errors,"Refs %d,  Reused %d\n",count_references,duplicate_references);
	fclose(f_in);
	fclose(f_phdata);
	fclose(f_errors);
	fclose(f_phindex);
	fclose(f_phtab);
	fclose(f_phcontents);

	LoadPhData();
	LoadVoice(voice_name2,0);
	Report();
	DrawEnvelopes();

	report_dict = CompileAllDictionaries();

	if(gui_flag)
	{
		delete progress;
	}

	report.Printf(_T("Compiled phonemes: %d errors."),error_count);
	if(error_count > 0)
	{
		report += _T(" See file: 'error_log'.");
		wxLogError(report);
	}
	wxLogStatus(report + report_dict);

	if(gui_flag == 0)
	{
		strncpy0(fname,(report+report_dict).mb_str(wxConvLocal),sizeof(fname));
		fprintf(stderr,"%s\n",fname);
		
	}
}  // end of Compile::CPhonemeTab



void CompileMbrola()
{//=================
	char *p;
	FILE *f_in;
	FILE *f_out;
	int percent;
	int n;
	int *pw;
	int *pw_end;
	int count = 0;
	int control;
	char phoneme[40];
	char phoneme2[40];
	char name1[40];
	char name2[40];
	char mbrola_voice[40];
	char buf[sizeof(path_home)+30];
	int mbrola_ctrl = 20;   // volume in 1/16 ths
	MBROLA_TAB data[N_PHONEME_TAB];

	wxString filepath = wxFileSelector(_T("Read Mbrola phonemes file"),path_phsource+_T("/mbrola"),_T(""),_T(""),_T("*"),wxOPEN);
	strcpy(buf,filepath.mb_str(wxConvLocal));
	if((f_in = fopen(buf,"r")) == NULL)
	{
		wxLogError(_T("Can't read: ")+filepath);
		return;
	}

	while(fgets(buf,sizeof(phoneme),f_in) != NULL)
	{
		buf[sizeof(phoneme)-1] = 0;

		if((p = strstr(buf,"//")) != NULL)
			*p = 0;   // truncate line at comment

		if(memcmp(buf,"volume",6)==0)
		{
			mbrola_ctrl = atoi(&buf[6]);
			continue;
		}

		n = sscanf(buf,"%d %s %s %d %s %s",&control,phoneme,phoneme2,&percent,name1,name2);
		if(n >= 5)
		{
			data[count].name = StringToWord(phoneme);
			if(strcmp(phoneme2,"NULL")==0)
				data[count].next_phoneme = 0;
			else
			if(strcmp(phoneme2,"VWL")==0)
				data[count].next_phoneme = 2;
			else
				data[count].next_phoneme = StringToWord(phoneme2);
			data[count].mbr_name = 0;
			data[count].mbr_name2 = 0;
			data[count].percent = percent;
			data[count].control = control;
			if(strcmp(name1,"NULL")!=0)
				data[count].mbr_name = StringToWord(name1);
			if(n == 6)
				data[count].mbr_name2 = StringToWord(name2);

			count++;
		}
	}
	fclose(f_in);

	wxFileName filename = wxFileName(filepath);
	strcpy(mbrola_voice,filename.GetName().mb_str(wxConvLocal));
	sprintf(buf,"%s/mbrola_ph/%s_phtrans",path_home,mbrola_voice);
	if((f_out = fopen(buf,"w")) == NULL)
	{
		wxLogError(_T("Can't write to: ")+wxString(buf,wxConvLocal));
		return;
	}
	
	data[count].name = 0;  // list terminator
	Write4Bytes(f_out, mbrola_ctrl);

	pw_end = (int *)(&data[count+1]);
	for(pw = (int *)data; pw < pw_end; pw++)
	{
		Write4Bytes(f_out, *pw);
	}
	fclose(f_out);
	wxLogStatus(_T("Mbrola translation file: %d phonemes"),count);
}  // end of CompileMbrola



void CompileInit(void)
{
	compile = new Compile;
	compile->CPhonemeTab("phonemes");
	delete compile;
}
