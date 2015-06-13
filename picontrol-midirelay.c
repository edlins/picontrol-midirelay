#include <alsa/asoundlib.h>
#include <wiringPi.h>
#include <limits.h>
#include <unistd.h>
#include <math.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static snd_seq_t *seq_handle;
static int in_port;
static int out_port;

////////////////////////////////////////////////////////////////////////////
//Example setup: There are 12 melody channels. Each index is mapped to 
// the corresponding Wiring Pi valued channel in the array below.
//
//////////////////////////////////////////////////////////////////

#define NUMPINS 8

#define NUMDIRECTKEYS 8
#define NUMSEQKEYS 5
#define NUMLOOPKEYS 5

int directKeys[NUMDIRECTKEYS] = { 36, 38, 40, 41, 43, 45, 47, 48 };
int seqKeys[NUMSEQKEYS] = { 49, 51, 54, 56, 58 };
int loopKeys[NUMLOOPKEYS] = { 37, 39, 42, 44, 46 };

int pinMapping[] = { 0, 1, 2, 3, 7, 6, 5, 4 };

// notes and channels playing on each pin
int pinNotes[NUMPINS];
int pinChannels[NUMPINS];

// seqs and loops playing from each key
int keyLoops[NUMLOOPKEYS];
int keySeqs[NUMSEQKEYS];

//Enabled channels
int playChannels[16];

#define THRUPORTCLIENT 14
#define THRUPORTPORT 0

#define DATA	27
#define CLOCK	28
#define LATCH	29

char *seqFiles[] = { "seq01.mid", "seq02.mid", "seq03.mid", "seq04.mid", "seq05.mid" };
char *loopFiles[] = { "loop01.mid", "loop02.mid", "loop03.mid", "loop04.mid", "loop05.mid" };

static void sigchld_hdl(int sig) {
  int pid = waitpid(-1,NULL,WNOHANG);
  //printf("PID %d\n", pid);
  while (pid > 0) {
    int i;
    for (i = 0; i < NUMSEQKEYS; i++) {
      //printf("%d ", keySeqs[i]);
      if (keySeqs[i] == pid) {
        printf("sequence ended on %d\n", seqKeys[i]);
        keySeqs[i] = -1;
      } // if pid matches
    } // for
    //printf("\n");
    pid = waitpid(-1,NULL,WNOHANG);
    //printf("*PID %d\n", pid);
  } // while more pids
}  // sigchld_hdl

void shift_out(unsigned int data) {
  unsigned int i;
  //printf("Shifting %x\n",data);
  for (i=0; i<8; i++) {
    if (0==(data & (0x01 << i))) {
      digitalWrite(DATA, LOW);
      printf("0");
    } else {
      digitalWrite(DATA, HIGH);
      printf("1");
    }
    digitalWrite(CLOCK, HIGH);
    //delay(1);
    delayMicroseconds(1);
    digitalWrite(CLOCK, LOW);
  }
}


void midi_open(void)
{
  //snd_seq_open(&seq_handle, "default", SND_SEQ_OPEN_INPUT, 0);
  snd_seq_open(&seq_handle, "default", SND_SEQ_OPEN_DUPLEX, 0);
  snd_seq_set_client_name(seq_handle, "LightOrgan");
  in_port = snd_seq_create_simple_port(seq_handle, "listen:in",
                    SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,
                    SND_SEQ_PORT_TYPE_APPLICATION);
  out_port = snd_seq_create_simple_port(seq_handle, "write:out",
                    SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ,
                    SND_SEQ_PORT_TYPE_APPLICATION);
  //if( snd_seq_connect_from(seq_handle, in_port, THRUPORTCLIENT, THRUPORTPORT) == -1) {
     //perror("Can't connect to thru port");
     //exit(-1);
  //} 
  system("aconnect LPK25 LightOrgan");
}


snd_seq_event_t *midi_read(void)
{
    snd_seq_event_t *ev = NULL;
    snd_seq_event_input(seq_handle, &ev);
    return ev;
}



void clearPinNotes() {
   int i;
   for(i = 0; i < NUMPINS; i++) {
      pinNotes[i] = -1;
   }
}

void clearPinChannels() {
   int i;
   for(i=0; i< NUMPINS; i++) {
      pinChannels[i] = INT_MAX;
   }
}

void clearKeyLoops() {
   int i;
   for(i = 0; i < NUMLOOPKEYS; i++) {
      keyLoops[i] = -1;
   }
}

void clearKeySeqs() {
   //printf("clearKeySeqs\n");
   int i;
   for(i = 0; i < NUMSEQKEYS; i++) {
      keySeqs[i] = -1;
   }
}


void myShiftOut() {
  int numBufs = (NUMPINS - 1) / 8;
  int pinsUsed = 8;
  int p;
  int b;
  int buffer;

  for (b = 0; b < numBufs + 1; b++) {
    pinsUsed = 8;
    if (b == numBufs) {
      pinsUsed = NUMPINS % 8;
      if (pinsUsed == 0) {
        pinsUsed = 8;
      } // if pinsUsed == 0
    } // if b == numBufs

    buffer = 0;
    for (p = 0; p < pinsUsed; p++) {
      if (pinNotes[p + b * 8] != -1) {
        buffer = buffer << 1 | 1;
      } else {
        buffer = buffer << 1 | 0;
      } // else
    } // for p

    //printf("%d: %x\n",b,buffer);
    shift_out(buffer);
  } // for b
  digitalWrite(LATCH, HIGH);
  //delay(1);
  delayMicroseconds(100);
  digitalWrite(LATCH, LOW);
  printf("\n");
} // myShiftOut


void clearPinsState() {
  clearPinNotes();
  clearPinChannels();
  clearKeyLoops();
  clearKeySeqs();
}


void setChannelInstrument(int channel, int instr) {
  //printf("setting channel %i to instrument %i\n", channel, instr);
  playChannels[channel] = instr;  
}


int isPercussion(int instrVal) {
  return instrVal >= 8 && instrVal <= 15;
}

int isPercussionChannel(int channel) {
  int instr = playChannels[channel];
  return isPercussion(instr);
}


int isBase(int instrVal) {
  return instrVal >= 32 && instrVal <= 39;
}
int isSynth(int instrVal) {
  return instrVal >= 88 && instrVal <= 103;
}



int choosePinIdx(int note, int channel) {
   //For folding all notes into available pins
   //Return the note modulated by the number of melody pins
   //int val = note  % NUMPINS;
   //return val;

   //For using only 8 specific notes for 8 pins
   int i;
   for (i = 0; i < NUMDIRECTKEYS; i++) {
     if (directKeys[i] == note) {
       return pinMapping[i];
     } // if directKey
   } // for
   return -1;
} // choosePinIdx


void midi_process(snd_seq_event_t *ev) {
  printf("EVENT %d ", ev->type);
  //If this event is a PGMCHANGE type,
  //it's a request to map a channel to an instrument
  if(ev->type == SND_SEQ_EVENT_PGMCHANGE) {
/*
    printf("PGMCHANGE: channel %2d, %5d, %5d\n",
            ev->data.control.channel,
            ev->data.control.param,
            ev->data.control.value);
    //Clear pins state, this is probably the beginning of a new song
    //clearPinsState();
*/
    setChannelInstrument(ev->data.control.channel, ev->data.control.value);
    snd_seq_drain_output(seq_handle);
    return;
  } //if PGMCHANGE

  if (isPercussionChannel(ev->data.note.channel)) {
    printf("skipping percussion\n");
    snd_seq_drain_output(seq_handle);
    return;
  } //if percussion

  if (ev->type != SND_SEQ_EVENT_NOTEON && ev->type != SND_SEQ_EVENT_NOTEOFF) {
    //printf("unhandled event %d\n", ev->type);
    snd_seq_drain_output(seq_handle);
    return;
  } //not noteon or noteoff
  int i;

  printf("%d, %d, %d, %d ",
          ev->data.note.note,
          ev->data.note.channel,
          ev->source.client,
          ev->source.port);
/*
  for (i = 0; i < NUMSEQKEYS; i++) {
    if (keySeqs[i] != -1) {
      printf("1");
    }
    else {
      printf("0");
    }
  }
  printf("\n");
*/

  //velocity == 0 means the same thing as a NOTEOFF type event
  int isOn = 1;
  if(ev->data.note.velocity == 0 ||
     ev->type == SND_SEQ_EVENT_NOTEOFF) {
    isOn = 0;
  } //if velocity == 0

  // is it a directNote?
  int directNote = 0;
  for (i = 0; i < NUMDIRECTKEYS; i++) {
    //printf("%d\n",directKeys[i]);
    if (ev->data.note.note == directKeys[i]) {
      directNote = 1;
      break;
    } //if directNote
  } //for loop

  // is it a seqNote?
  int seqNote = 0;
  if (!directNote) {
    for (i = 0; i < NUMSEQKEYS; i++) {
      if (ev->data.note.note == seqKeys[i]) {
        seqNote = 1;
        break;
      } // if seqNote
    } // for
  } // if not directNote

  // is it a loopNote?
  int loopNote = 0;
  if (!directNote && !seqNote) {
    for (i = 0; i < NUMLOOPKEYS; i++) {
      if (ev->data.note.note == loopKeys[i]) {
        loopNote = 1;
        break;
      } // if seqNote
    } // for
  } // if not directNote and not seqNote


  //
  // if it is a directNote, turn the pin on or off
  //
  if (directNote && ev->data.note.note != 0) {
    //choose the output pin based on the pitch of the note
    int pinIdx = choosePinIdx(ev->data.note.note, ev->data.note.channel);
    if (isOn) {
      pinNotes[pinIdx] = ev->data.note.note;
      pinChannels[pinIdx] = ev->data.note.channel;
    } // if isOn
    else {
      pinNotes[pinIdx] = -1;
      pinChannels[pinIdx] = INT_MAX;
    } // else
/*
    snd_seq_ev_set_note(snd_seq_ev_set_fixed(ev),
                        ev->data.note.channel,
                        ev->data.note.note,
                        127,
                        ev->data.note.duration);
*/
    ev->data.note.velocity = 127;
    snd_seq_ev_set_source(ev, out_port);
    snd_seq_ev_set_subs(ev);
    snd_seq_ev_set_direct(ev);
    snd_seq_event_output(seq_handle, ev);
    snd_seq_drain_output(seq_handle);
    myShiftOut();
  } // if directNote


  //
  // if it is a seqNote, start a sequence
  //
  else if (seqNote) {
    //if (keySeqs[i] == -1) {
    if (isOn) {
      int childPID = fork();
        if (childPID == 0) {
          // child
          char *file = seqFiles[i];
          char command[100];
          printf("starting %s on %d\n", file, seqKeys[i]); 

/* midish - slow startup, latency
          int tempo = 100;
          sprintf(command, "echo \'dnew 0 \"128:0\" wo; import \"/root/picontrol-midirelay/mid/%s\"; fac %d; p; s; exit;\' | midish -b", file, tempo);
          printf("command: %s\n", command);
          int rc = system(command);
*/

/* aplaymidi - no tempo control */
          sprintf(command, "aplaymidi /root/picontrol-midirelay/mid/%s --port 128 -d 0", file);
          printf("command: %s\n", command);
          int rc = system(command);

/* playmidi - no concurrent execution
          sprintf(command, "playmidi -e /root/picontrol-midirelay/mid/%s", file);
          char filep[100];
          sprintf(filep,"/root/picontrol-midirelay/mid/%s", file);
          execl("/usr/bin/playmidi", "/usr/bin/playmidi", "-e", filep, (char *)NULL);
*/
          
          printf("exiting\n");
          exit(0);
        } // if child
        else {
          //printf("starting child %d\n", childPID);
          keySeqs[i] = childPID;
        } // else parent
    } // if not already playing that sequence
    else {
      // just lifted the seq key, nothing to do
    } // else
    snd_seq_drain_output(seq_handle);
  } // else if seqNote


  //
  // if it is a loopNote, start or stop a loop
  //
  else if (loopNote) {
    if (isOn) {
      if (keyLoops[i] == -1) {
        int childPID = fork();
        if (childPID == 0) {
          // child
          char *file = loopFiles[i];
          char command[100];
          printf("starting %s on %d\n", file, loopKeys[i]);
          sprintf(command, "aplaymidi /root/picontrol-midirelay/mid/%s --port 128 -d 0", file);
          //sprintf(command, "playmidi -e /root/picontrol-midirelay/mid/%s", file);
          //printf("command: %s\n", command);
          while (1) {
            int rc = system(command);
          } //while forever
        } //if child
        else {
          // parent
          //printf("saving childPid %d\n", childPID);
          keyLoops[i] = childPID;
        } //if parent
      } // if not playing already
      else {
        //printf("killing childPID %d\n", keyLoops[i]);
        printf("ending loop on %d\n", loopKeys[i]);
        int rc = kill(keyLoops[i],9);
        keyLoops[i] = -1;
      } // if was playing
    } // if isOn
    snd_seq_drain_output(seq_handle);
  } // else if loopNote

} // midi_process


int main() {
  struct sigaction act;
  memset(&act,0,sizeof(act));
  act.sa_handler = &sigchld_hdl;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_RESTART | SA_NOCLDSTOP;
  if (sigaction(SIGCHLD,&act,0) == -1) {
    perror("sigaction");
    printf("sigaction\n");
    return 1;
  }

    //Setup wiringPi
    if( wiringPiSetup() == -1) {
      exit(1);
    }
   
    //setup the SPI pins
    pinMode(DATA,OUTPUT);
    pinMode(CLOCK,OUTPUT);
    pinMode(LATCH,OUTPUT);
    digitalWrite(DATA,LOW);
    digitalWrite(CLOCK,LOW);
    digitalWrite(LATCH,LOW);

    clearPinsState();

    //Open a midi port, connect to thru port also
    midi_open();

    //Process events forever
    while (1) {
       midi_process(midi_read());
    }

    return -1;
}
