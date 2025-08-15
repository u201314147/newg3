#include <common.h>
#include "global.h"
#include "hooks.c"
#include "menu.c"
#include "states.c"
#include "spectator_mode.c"
#include "zMirrorMode.c"
#include "names3d.c"
#include "bluefire.c"
#include "pMoonGravity.c"
#include "thread.c"
#include "lapdata.c"



typedef void (*VehicleFuncPtr)(struct Thread* thread, struct Driver* driver);

void RunVehicleThread(VehicleFuncPtr func, struct Thread* thread, struct Driver* driver)
{
    UpdateCheckpointTracker(driver->driverID);
    if (func == nullptr) { return; }

    bool restore = false;
    if ((sdata->gGT->gameMode1 & END_OF_RACE) && (checkpointTracker[driver->driverID].raceFinished == 0))
    {
        sdata->gGT->gameMode1 &= ~(END_OF_RACE);
        restore = true;
    }
    func(thread, driver);
    if (restore) { sdata->gGT->gameMode1 |= END_OF_RACE; }
}

void RunVehicleSet13(struct Thread* dThread, struct Driver* dOnline)
{
	if(dThread->funcThTick != 0)
		return;

	// This only works because we have NO collision
	// and therefore NO bounce physics, update P1
	// first, then update other drivers later
	
	// We can still "eventually" have mask-weapon
	// collision, but no "bounce" physics with this logic
		
	for(int iVar11 = 0; iVar11 < 13; iVar11++)
	{
		int pcVar5 = dOnline->funcPtrs[iVar11];
		
		if(pcVar5 == 0)
			continue;
		
		RunVehicleThread(pcVar5, dThread, dOnline);
		
		#ifdef USE_60FPS
			#ifndef REBUILD_PS1
				// if this function just ran
				if(pcVar5 == VehFrameProc_Driving)
				{
					// only if jumping animation,
					// otherwise wheelie gets bugged
					if(dOnline->instSelf->animIndex == 3)
					{
						dOnline->matrixIndex =
						dOnline->matrixIndex >> 1;
					}
				}
			#endif
		#endif
	}
}

void FixReservesIncrement(struct Driver * driver, int reserves)
{
	if (driver->reserves > 30000) { driver->uncappedReserves += reserves; }
	else { driver->reserves += reserves; }
}


void Online_CollidePointWithBucket(struct Thread* th, short* vec3_pos)
{
    // disable collisions in special 3 
    if (octr->special == 3) { 
        return;
    }
	else 
	{
	while (th != 0)
	{
		DECOMP_PROC_CollidePointWithSelf(th, vec3_pos);
		// next
		th = th->siblingThread;
	}
  }
}

unsigned int checkpointTimes[(MAX_LAPS * CPS_PER_LAP) + 1];
#ifndef REBUILD_PC
void AssignMeterGrade(struct Driver * driver, int meterLeft)
{
	const int gradeTreshold[] = {SECONDS(1) * FP(0.50), SECONDS(1) * FP(0.65),
						SECONDS(1) * FP(0.80), SECONDS(1) * FP(0.90),
						SECONDS(1) * FP(0.95)};
	const int gradeColors[] = {CORTEX_RED, ROO_ORANGE, CRASH_BLUE, TROPY_LIGHT_BLUE, PAPU_YELLOW};

	driver->meterGradeTimer = SECONDS(0.5);
	driver->meterGrade[1] = '\0';

	char grades[] = "FDCBA";
	for (int i = 0; i < len(gradeTreshold); i++)
	{
		if (meterLeft > SECONDS(1) - FP_INT(gradeTreshold[i]))
		{
			driver->meterGrade[0] = grades[i];
			driver->gradeColor = gradeColors[i];
			return;
		}
	}
	driver->meterGrade[0] = 'S';
	driver->gradeColor = SILVER;
}
#endif

extern const char specting[];
extern const char finish_race[];

int shouldExecuteSpecText = 0;

const char* spec_mode;

void spec_text() {
        spec_mode = shouldExecuteSpecText ? specting : finish_race;
        static unsigned frameCounter = 0;
        int spec_color = frameCounter++ & FPS_DOUBLE(1) ? ORANGE : WHITE;
        DECOMP_DecalFont_DrawLine(spec_mode, 0x100, 0x74, FONT_SMALL, JUSTIFY_CENTER | spec_color);

    
}

void ban_demo_skip()
{       

if (octr->special == 8) {
    data.gamepadMapBtn[8].output = BTN_R2; // if demo camera mode change L2 to R2
}

	if (octr->CurrState < LOBBY_WAIT_FOR_LOADING)
	{
	  data.gamepadMapBtn[8].output = BTN_L2_one; // if not in demo camera mode use L2 as L2

	} 
	
	return;
}







