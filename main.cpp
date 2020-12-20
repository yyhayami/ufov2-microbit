#include "MicroBit.h"

#define GAME_ON         0
#define GAME_OVER       1

struct Point
{
    int     x;
    int     y;
};

static MicroBit        uBit;
static MicroBitImage   ufos(5,5);
static int             score;
static int             game_over;
static int             level;
static int             UFO_SPEED = 750;
static int             PLAYER_SPEED = 150;
static int             BULLET_SPEED = 50;
static Point           player;
static Point           bullet;
static NRF52ADCChannel *mic = NULL;
static StreamNormalizer *processor = NULL;
static LevelDetector *leveld = NULL;
static int claps = 0;

const uint8_t ufo[][25] = {{0, 0, 255, 0, 0, //Level 1 UFO
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0},
{0, 0, 255, 255, 0,                         //Level 2 UFO
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0},
{0, 255, 255, 255, 0,                       //Level 3 UFO
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0},
{0, 0, 255, 255, 0,                         //Level 4 UFO
    0, 0, 255, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0},
{0, 0, 255, 255, 0,                         //Level 5 UFO
    0, 0, 255, 255, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0},
{0, 255, 255, 255, 0,                       //Level 6 UFO
    0, 0, 255, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0},
{0, 255, 255, 255, 0,                       //Level 7 UFO
    0, 255, 255, 255, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0}
};

const uint8_t exps[][25] = {{0, 0, 0, 0, 0,
    0, 0, 255, 0, 0,
    0, 255, 0, 255, 0,
    0, 0, 255, 0, 0,
    0, 0, 0, 0, 0},
{0, 0, 0, 0, 0,
    0, 255, 255, 255, 0,
    0, 255, 0, 255, 0,
    0, 255, 255, 255, 0,
    0, 0, 0, 0, 0},
{255, 255, 255, 255, 255,
    255, 0, 0, 0, 255,
    255, 0, 0, 0, 255,
    255, 0, 0, 0, 255,
    255, 255, 255, 255, 255}
};

const ManagedString names[] = {
        ManagedString("soaring"),	//0
        ManagedString("hello"),	    //1
        ManagedString("hello"),		//2
        ManagedString("slide"),		//3
        ManagedString("slide"),		//4
        ManagedString("slide"),		//5
        ManagedString("spring"),	//6
        ManagedString("spring"),	//7
        ManagedString("twinkle"),	//8
        ManagedString("yawn"),		//9
        ManagedString("happy"),     //10
        ManagedString("sad"),		//11
        ManagedString("010232279000001440226608881023012800000000240000000000000000000000000000,310231623023602440093908880000012800000100240000000000000000000000000000"), //12
        ManagedString("010232279000001440226608881023012800000000240000000000000000000000000000,310232226010801440162408881023012800000100240000000000000000000000000000"), //13
        ManagedString("010230988019008440044008881023001601003300240000000000000000000000000000,110232570087411440044008880352005901003300010000000000000000010000000000"), //14
        ManagedString("")
};

void 
setPixel(const uint8_t *pt)
{
	for (int y=0; y < 5; y++) 
		for (int x=0; x < 5; x++) {
			ufos.setPixelValue(x,y,*(pt+y*5+x));
	
	}	
}

void
explosion() {
    uBit.sleep(80);
    setPixel(exps[0]);
    uBit.audio.soundExpressions.playAsync(names[14]);
    uBit.display.image.paste(ufos);
    uBit.sleep(80);
    setPixel(exps[1]);
    uBit.display.image.paste(ufos);
    uBit.sleep(80);
    setPixel(exps[2]);
    uBit.display.image.paste(ufos);
    uBit.sleep(100);
    ufos.clear();
    uBit.display.image.paste(ufos);
    uBit.sleep(100);
}


int
addRow()
{
    // If we're adding a row of ufos, but we're out of space, it's game over!!
    for (int x=0; x<5; x++)
        if (ufos.getPixelValue(x,4))
            return GAME_OVER;

    // Otherwise, move down the ufos, and add a new role at the top.
    ufos.shiftDown(1);

    return GAME_ON;
}

int
addUfo()
{
    if (level <= 7) {
        uBit.audio.soundExpressions.playAsync(names[level]);
        setPixel(ufo[level-1]);
    } else {
        return GAME_OVER;
    }
    
    return GAME_ON;
}

bool
ufosInColumn(int x)
{
    for (int y = 0; y < 5; y++)
        if (ufos.getPixelValue(x,y))
            return true;

    return false;
}

bool
ufoCount()
{
    int count = 0;

    for (int x=0; x<5; x++)
        for (int y=0; y<5; y++)
            if (ufos.getPixelValue(x,y))
                count++;

    return count;
}

void
ufoUpdate()
{
    bool movingRight = true;

    while(!game_over) {
        // Wait for next update;    
        uBit.sleep(UFO_SPEED);

        if (ufoCount() == 0) {
            explosion();
            level++;
            if (addUfo() == GAME_OVER) {
                game_over = true;
                return;
            }
        }
        
        if (movingRight) {
            if(ufosInColumn(4)) {
                movingRight = false;
                if (addRow() == GAME_OVER) {
                    game_over = true;
                    return;
                }
            } else {
                ufos.shiftRight(1);
            }
        } else {
            if(ufosInColumn(0)) {
                movingRight = true;
                if (addRow() == GAME_OVER) {
                    game_over = true;
                    return;
                }
            } else {
                ufos.shiftLeft(1);
            }
        }
    }
}


void
bulletUpdate()
{
    while (!game_over) {
        uBit.sleep(BULLET_SPEED);
        if (bullet.y != -1)
            bullet.y--;

        if (ufos.getPixelValue(bullet.x, bullet.y) > 0) {
            score++;
            ufos.setPixelValue(bullet.x, bullet.y, 0);
            bullet.x = -1;
            bullet.y = -1;
            uBit.audio.soundExpressions.playAsync(names[12]);
        }
    }
}

void
playerUpdate()
{
    while (!game_over) {
        uBit.sleep(PLAYER_SPEED);

        if(uBit.accelerometer.getX() < -300 && player.x > 0)
            player.x--;

        if(uBit.accelerometer.getX() > 300 && player.x < 4)
            player.x++;
    }
}

void
fire(MicroBitEvent)
{
    if (bullet.y == -1) {
        bullet.y = 4;
        bullet.x = player.x;
	    uBit.audio.soundExpressions.playAsync(names[13]);
    }
}

void micReset();

void
onLoud(MicroBitEvent)
{
    claps++;
    if (claps>1 && claps<10 && level>1) {
        ufos.clear();
        claps = 10;
        micReset();
    }
}

void
onQuiet(MicroBitEvent)
{
}

void micSet()
{
    mic = uBit.adc.getChannel(uBit.io.microphone);
    mic->setGain(7,0);

    processor = new StreamNormalizer(mic->output, 1.0f, true, DATASTREAM_FORMAT_UNKNOWN, 10);

    leveld = new LevelDetector(processor->output, 400, 150);
    uBit.io.runmic.setDigitalValue(1);
    uBit.io.runmic.setHighDrive(true);

    uBit.messageBus.listen(DEVICE_ID_SYSTEM_LEVEL_DETECTOR, LEVEL_THRESHOLD_HIGH, onLoud);
    uBit.messageBus.listen(DEVICE_ID_SYSTEM_LEVEL_DETECTOR, LEVEL_THRESHOLD_LOW, onQuiet);
}


void 
micReset() {
    uBit.messageBus.ignore(DEVICE_ID_SYSTEM_LEVEL_DETECTOR, LEVEL_THRESHOLD_HIGH, onLoud);
    uBit.messageBus.ignore(DEVICE_ID_SYSTEM_LEVEL_DETECTOR, LEVEL_THRESHOLD_LOW, onQuiet);
    
    uBit.io.runmic.setDigitalValue(0);
    uBit.io.runmic.setHighDrive(false);
}

void 
ufoGo()
{   
    ufos.clear();
    addUfo();

    micSet();
    create_fiber(ufoUpdate);
    create_fiber(bulletUpdate);
    create_fiber(playerUpdate);

    // Register event handlers for button presses (either button fires!)
    uBit.messageBus.listen(MICROBIT_ID_BUTTON_A, MICROBIT_BUTTON_EVT_CLICK, fire);
    uBit.messageBus.listen(MICROBIT_ID_BUTTON_B, MICROBIT_BUTTON_EVT_CLICK, fire);

    // Now just keep the screen refreshed.
    while (!game_over) {    
        uBit.sleep(10);
        uBit.display.image.paste(ufos);
        uBit.display.image.setPixelValue(player.x, player.y, 255);
        uBit.display.image.setPixelValue(bullet.x, bullet.y, 255);
    }
}

void
gameOver()
{
    if (level < 7) {
        explosion();
    }
    uBit.display.clear();
    if (score==23) {
        uBit.audio.soundExpressions.playAsync(names[10]);
        uBit.display.scroll("Perfect!:");
    } else {
        uBit.audio.soundExpressions.playAsync(names[11]);
        uBit.display.scroll("GAME OVER:");
    }
    uBit.display.scroll(score);
}

int main()
{
    // Reset all game state.
    game_over = 0;
    level = 1;
    score = 0;
    player.x = 2;
    player.y = 4;

    bullet.x = -1;
    bullet.y = -1;
    
    // Initialise the micro:bit runtime.
	uBit.init();

    // Welcome message
    uBit.audio.setVolume(128);
    uBit.audio.soundExpressions.playAsync(names[0]);
    uBit.display.scroll("UFO!!!");
    // Keep playing forever
    ufoGo();
    // Display GAME OVER and score
    gameOver();
}
