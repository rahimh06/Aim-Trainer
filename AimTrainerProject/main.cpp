#include "raylib.h"
#include <iostream>
#include <fstream>
#include <cmath>
#include <ctime>
#include <string>

using namespace std;

struct AimTarget
{
    float posX, posY;
    float size;
    bool alive;
    double created;
    Color shade;
};

struct ClickAnimation
{
    float centerX, centerY;
    float currentSize;
    double beginTime;
    bool showing;
    float finalSize;
};

class SpawnPoint
{
public:
    double when;
    float coordX, coordY;
    SpawnPoint* following;
    
    SpawnPoint()
    {
        when = 0.0;
        coordX = 0.0;
        coordY = 0.0;
        following = NULL;
    }
};

class TargetQueue
{
private:
    SpawnPoint* first;
    SpawnPoint* last;
    
public:
    TargetQueue()
    {
        first = NULL;
        last = NULL;
    }
    
    void addPoint(double moment, float xPos, float yPos)
    {
        SpawnPoint* fresh = new SpawnPoint;
        fresh->when = moment;
        fresh->coordX = xPos;
        fresh->coordY = yPos;
        fresh->following = NULL;
        
        if(last == NULL)
        {
            last = fresh;
            first = last;
        }
        else
        {
            last->following = fresh;
            last = fresh;
        }
    }
    
    SpawnPoint* takePoint()
    {
        if(first == NULL) return NULL;
        
        SpawnPoint* temp = first;
        first = first->following;
        
        if(first == NULL)
        {
            last = NULL;
        }
        
        return temp;
    }
    
    SpawnPoint* checkNext()
    {
        return first;
    }
    
    bool empty()
    {
        return first == NULL;
    }
    
    void wipe()
    {
        while(first != NULL)
        {
            SpawnPoint* temp = first;
            first = first->following;
            delete temp;
        }
        last = NULL;
    }
    
    void prepare(int mode)
    {
        wipe();
        
        float rad = 40;
        int rBound = (int)rad;
        int idx;
        double timer = 0.0;
        
        for(idx = 0; idx < 60; idx++)
        {
            float spawnX = rad + 10 + rand() % (1600 - 2 * rBound - 20);
            float spawnY = 160 + rad + rand() % (900 - 160 - 2 * rBound - 10);
            addPoint(timer, spawnX, spawnY);
            
            double waitTime = 1.0;
            
            if(mode == 0)
            {
                if(timer < 10.0) waitTime = 1.5;
                else if(timer < 20.0) waitTime = 1.2;
                else waitTime = 1.0;
            }
            else if(mode == 1)
            {
                if(timer < 10.0) waitTime = 1.0;
                else if(timer < 20.0) waitTime = 0.85;
                else waitTime = 0.65;
            }
            else
            {
                if(timer < 10.0) waitTime = 0.8;
                else if(timer < 20.0) waitTime = 0.6;
                else waitTime = 0.4;
            }
            
            timer += waitTime;
        }
    }
};

struct RecordEntry
{
    int points;
    int occurrences;
    RecordEntry* nextRecord;
};

class ScoreRecords
{
private:
    RecordEntry* records[60];
    int totalSlots;
    
public:
    ScoreRecords()
    {
        totalSlots = 60;
        int idx;
        for(idx = 0; idx < totalSlots; idx++)
        {
            records[idx] = NULL;
        }
    }
    
    int hashScore(int score)
    {
        return score % totalSlots;
    }
    
    void storeScore(int score)
    {
        int slot = hashScore(score);
        RecordEntry* current = records[slot];
        
        while(current != NULL)
        {
            if(current->points == score)
            {
                current->occurrences += 1;
                return;
            }
            current = current->nextRecord;
        }
        
        RecordEntry* newEntry = new RecordEntry;
        newEntry->points = score;
        newEntry->occurrences = 1;
        newEntry->nextRecord = records[slot];
        records[slot] = newEntry;
    }
    
    int findScore(int score)
    {
        int slot = hashScore(score);
        RecordEntry* current = records[slot];
        
        while(current != NULL)
        {
            if(current->points == score)
                return current->occurrences;
            current = current->nextRecord;
        }
        
        return 0;
    }
    
    void bestFive(int topScores[], int topCounts[])
    {
        int i, j;
        for(i = 0; i < 5; i++)
        {
            topScores[i] = -1;
            topCounts[i] = 0;
        }
        
        for(i = 0; i < totalSlots; i++)
        {
            RecordEntry* entry = records[i];
            while(entry != NULL)
            {
                int scoreVal = entry->points;
                int freq = entry->occurrences;
                
                for(j = 0; j < 5; j++)
                {
                    if(freq > topCounts[j])
                    {
                        int k;
                        for(k = 4; k > j; k--)
                        {
                            topScores[k] = topScores[k - 1];
                            topCounts[k] = topCounts[k - 1];
                        }
                        topScores[j] = scoreVal;
                        topCounts[j] = freq;
                        break;
                    }
                }
                entry = entry->nextRecord;
            }
        }
    }
};

class GameSession
{
public:
    int targetsHit;
    int shotsFired;
    double averageSpeed;
    double averagePrecision;
    string timestamp;
    GameSession* nextSession;
    
    GameSession()
    {
        targetsHit = 0;
        shotsFired = 0;
        averageSpeed = 0.0;
        averagePrecision = 0.0;
        timestamp = "";
        nextSession = NULL;
    }
};

class SessionHistory
{
private:
    GameSession* start;
    
public:
    SessionHistory()
    {
        start = NULL;
    }
    
    void addSession(int hits, int shots, double speed, double precision, string time)
    {
        GameSession* newSession = new GameSession;
        newSession->targetsHit = hits;
        newSession->shotsFired = shots;
        newSession->averageSpeed = speed;
        newSession->averagePrecision = precision;
        newSession->timestamp = time;
        newSession->nextSession = start;
        start = newSession;
    }
    
    GameSession* getStart()
    {
        return start;
    }
    
    void loadSessions()
    {
        ifstream fileInput("performance.txt");
        if(!fileInput.is_open())
        {
            ofstream createFile("performance.txt");
            if(createFile.is_open())
            {
                createFile.close();
            }
            return;
        }
        
        int hitCount, shotCount;
        double speedVal, precisionVal;
        while(fileInput >> hitCount >> shotCount >> speedVal >> precisionVal)
        {
            string timeStr;
            getline(fileInput, timeStr);
            if(!timeStr.empty() && timeStr[0] == ' ')
                timeStr = timeStr.substr(1);
            addSession(hitCount, shotCount, speedVal, precisionVal, timeStr);
        }
        fileInput.close();
    }
    
    void saveSessions()
    {
        ofstream fileOutput("performance.txt", ios::trunc);
        GameSession* current = start;
        while(current)
        {
            fileOutput << current->targetsHit << " " << current->shotsFired << " " << current->averageSpeed << " " << current->averagePrecision << " " << current->timestamp << "\n";
            current = current->nextSession;
        }
        fileOutput.close();
    }
    
    void clearSessions()
    {
        GameSession* current = start;
        while(current)
        {
            GameSession* temp = current;
            current = current->nextSession;
            delete temp;
        }
        start = NULL;
    }
};

const int MAX_TARGETS = 60;
AimTarget gameTargets[MAX_TARGETS];
int activeTargets = 0;

const int MAX_ANIMATIONS = 20;
ClickAnimation clickAnimations[MAX_ANIMATIONS];

TargetQueue spawnQueue;
SessionHistory sessionLog;
ScoreRecords scoreLog;

int hitsTotal = 0;
int shotsTotal = 0;
double speedValues[MAX_TARGETS];
double precisionValues[MAX_TARGETS];
double roundStartTime = 0;

int streak = 0;
int bestStreak = 0;
int gameMode = 0;

Color targetColors[5] = { RED, BLUE, GREEN, ORANGE, PURPLE };
int selectedColor = 0;

float calculateDistance(float x1, float y1, float x2, float y2)
{
    return sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
}

void drawAimTarget(float centerX, float centerY, float radius, Color fill)
{
    DrawCircle(centerX, centerY, radius, fill);
    DrawCircle(centerX, centerY, radius * 0.75f, WHITE);
    DrawCircle(centerX, centerY, radius * 0.5f, fill);
    DrawCircle(centerX, centerY, radius * 0.25f, WHITE);
}

void clearTargets()
{
    int idx;
    for(idx = 0; idx < activeTargets; idx++)
    {
        gameTargets[idx].alive = false;
    }
    activeTargets = 0;
}

void createTarget()
{
    if(activeTargets >= MAX_TARGETS) return;
    if(spawnQueue.empty()) return;
    
    SpawnPoint* nextPoint = spawnQueue.checkNext();
    if(nextPoint == NULL) return;
    
    double timePassed = GetTime() - roundStartTime;
    
    if(timePassed >= nextPoint->when)
    {
        SpawnPoint* pointData = spawnQueue.takePoint();
        
        gameTargets[activeTargets].posX = pointData->coordX;
        gameTargets[activeTargets].posY = pointData->coordY;
        gameTargets[activeTargets].size = 40;
        gameTargets[activeTargets].alive = true;
        gameTargets[activeTargets].created = GetTime();
        gameTargets[activeTargets].shade = targetColors[selectedColor];
        activeTargets++;
        
        delete pointData;
    }
}

void createAnimation(float x, float y)
{
    int idx;
    for(idx = 0; idx < MAX_ANIMATIONS; idx++)
    {
        if(!clickAnimations[idx].showing)
        {
            clickAnimations[idx].centerX = x;
            clickAnimations[idx].centerY = y;
            clickAnimations[idx].currentSize = 5;
            clickAnimations[idx].finalSize = 25;
            clickAnimations[idx].beginTime = GetTime();
            clickAnimations[idx].showing = true;
            break;
        }
    }
}

void updateAnimations()
{
    int idx, point;
    for(idx = 0; idx < MAX_ANIMATIONS; idx++)
    {
        if(clickAnimations[idx].showing)
        {
            double elapsed = GetTime() - clickAnimations[idx].beginTime;
            
            int points = 5;
            float outerRing = 12;
            float innerRing = 5;
            
            for(point = 0; point < points * 2; point++)
            {
                float angle1 = (PI / points) * point;
                float angle2 = (PI / points) * (point + 1);
                float radius1 = (point % 2 == 0) ? outerRing : innerRing;
                float radius2 = ((point + 1) % 2 == 0) ? outerRing : innerRing;
                
                float x1 = clickAnimations[idx].centerX + cos(angle1) * radius1;
                float y1 = clickAnimations[idx].centerY + sin(angle1) * radius1;
                float x2 = clickAnimations[idx].centerX + cos(angle2) * radius2;
                float y2 = clickAnimations[idx].centerY + sin(angle2) * radius2;
                
                DrawTriangle(Vector2{ clickAnimations[idx].centerX, clickAnimations[idx].centerY },
                             Vector2{ x1, y1 }, Vector2{ x2, y2 }, ORANGE);
                DrawLineEx(Vector2{ x1, y1 }, Vector2{ x2, y2 }, 1.5f, YELLOW);
            }
            
            if(elapsed > 0.2)
            {
                clickAnimations[idx].showing = false;
            }
        }
    }
}

void loadBestStreak()
{
    ifstream fileIn("combo.txt");
    if(fileIn.is_open())
    {
        fileIn >> bestStreak;
        fileIn.close();
    }
    else
    {
        bestStreak = 0;
    }
}

void saveBestStreak()
{
    ofstream fileOut("combo.txt");
    fileOut << bestStreak;
    fileOut.close();
}

string getCurrentTime()
{
    time_t now = time(0);
    tm* localTime = localtime(&now);
    char timeBuffer[20];
    sprintf(timeBuffer, "%02d/%02d %02d:%02d", localTime->tm_mday, localTime->tm_mon + 1, localTime->tm_hour, localTime->tm_min);
    return string(timeBuffer);
}

double calculatePrecision(AimTarget target, float mouseX, float mouseY)
{
    float distance = calculateDistance(mouseX, mouseY, target.posX, target.posY);
    float radius = target.size;
    
    if(distance <= radius * 0.25f) return 1.0;
    else if(distance <= radius * 0.5f) return 0.8;
    else if(distance <= radius * 0.75f) return 0.6;
    else return 0.4;
}

void fillScoreData()
{
    GameSession* current = sessionLog.getStart();
    while(current)
    {
        scoreLog.storeScore(current->targetsHit);
        current = current->nextSession;
    }
}

int main()
{
    srand(time(0));
    InitWindow(1600, 900, "Aim Trainer");
    SetTargetFPS(60);
    
    int idx;
    for(idx = 0; idx < MAX_ANIMATIONS; idx++)
    {
        clickAnimations[idx].showing = false;
    }
    
    bool menuScreen = true;
    bool gameActive = false;
    bool historyScreen = false;
    bool colorScreen = false;
    bool countdownScreen = false;
    bool gameOverScreen = false;
    bool modeSelect = false;
    bool statsScreen = false;
    bool searchScreen = false;
    bool leaderboardScreen = false;
    
    int scoreInput = 0;
    bool typingActive = false;
    string typedText = "";
    string lastSearch = "";
    int searchResult = -1;
    
    double roundTime = 30.0;
    double timeElapsed = 0;
    double countdownBegin = 0;
    double gameOverBegin = 0;
    
    sessionLog.loadSessions();
    fillScoreData();
    loadBestStreak();
    
    Rectangle backButton = { 50, 800, 150, 50 };
    
    while(!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(BLACK);
        
        Vector2 mousePos = GetMousePosition();
        
        if(menuScreen)
        {
            DrawText("Aim Trainer", 600, 150, 50, WHITE);
            DrawText(TextFormat("Best Streak: %d", bestStreak), 620, 210, 30, GOLD);
            DrawText("Start Game", 650, 280, 40, SKYBLUE);
            DrawText("Session History", 650, 360, 40, LIME);
            DrawText("Search", 650, 440, 40, YELLOW);
            DrawText("Customize Color", 650, 520, 40, ORANGE);
            DrawText("Exit", 650, 600, 40, RED);
            
            if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                if(mousePos.x >= 650 && mousePos.x <= 950 && mousePos.y >= 280 && mousePos.y <= 320)
                {
                    menuScreen = false;
                    modeSelect = true;
                }
                if(mousePos.x >= 650 && mousePos.x <= 950 && mousePos.y >= 360 && mousePos.y <= 400)
                {
                    menuScreen = false;
                    historyScreen = true;
                }
                if(mousePos.x >= 650 && mousePos.x <= 950 && mousePos.y >= 440 && mousePos.y <= 480)
                {
                    menuScreen = false;
                    statsScreen = true;
                }
                if(mousePos.x >= 550 && mousePos.x <= 1050 && mousePos.y >= 520 && mousePos.y <= 560)
                {
                    menuScreen = false;
                    colorScreen = true;
                }
                if(mousePos.x >= 650 && mousePos.x <= 950 && mousePos.y >= 600 && mousePos.y <= 640)
                {
                    sessionLog.saveSessions();
                    saveBestStreak();
                    
                    double saveBegin = GetTime();
                    while(GetTime() - saveBegin < 1.0)
                    {
                        BeginDrawing();
                        ClearBackground(BLACK);
                        DrawText("Saving Files...", 600, 400, 50, WHITE);
                        EndDrawing();
                    }
                    
                    CloseWindow();
                    return 0;
                }
            }
        }
        else if(historyScreen)
        {
            DrawText("Session History", 600, 50, 50, WHITE);
            
            int verticalPos = 160;
            GameSession* current = sessionLog.getStart();
            while(current)
            {
                DrawText(TextFormat("%s | Hits=%d, Shots=%d, ShotAcc=%.1f%%, TargetAcc=%.1f%%",
                                    current->timestamp.c_str(), current->targetsHit, current->shotsFired,
                                    (current->shotsFired ? current->targetsHit * 100.0 / current->shotsFired : 0),
                                    current->averagePrecision * 100), 250, verticalPos, 25, WHITE);
                verticalPos += 40;
                current = current->nextSession;
            }
            
            DrawRectangleRec(backButton, LIGHTGRAY);
            DrawText("Back", backButton.x + 20, backButton.y + 10, 30, BLACK);
            
            if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                if(CheckCollisionPointRec(mousePos, backButton))
                {
                    historyScreen = false;
                    menuScreen = true;
                }
            }
        }
        else if(colorScreen)
        {
            DrawText("Customize Target Color", 550, 50, 50, WHITE);
            
            int startX = 350;
            int startY = 200;
            int spacing = 250;
            for(idx = 0; idx < 5; idx++)
            {
                float centerX = startX + idx * spacing;
                float centerY = startY;
                drawAimTarget(centerX, centerY, 40, targetColors[idx]);
                if(idx == selectedColor)
                {
                    DrawRectangleLines(centerX - 50, centerY - 50, 100, 100, WHITE);
                }
            }
            
            DrawRectangleRec(backButton, LIGHTGRAY);
            DrawText("Back", backButton.x + 20, backButton.y + 10, 30, BLACK);
            
            if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                if(CheckCollisionPointRec(mousePos, backButton))
                {
                    colorScreen = false;
                    menuScreen = true;
                }
                else
                {
                    for(idx = 0; idx < 5; idx++)
                    {
                        float centerX = startX + idx * spacing;
                        float centerY = startY;
                        if(calculateDistance(mousePos.x, mousePos.y, centerX, centerY) <= 40)
                        {
                            selectedColor = idx;
                        }
                    }
                }
            }
        }
        else if(statsScreen)
        {
            DrawText("Search", 600, 150, 50, WHITE);
            DrawText("Search Score", 650, 300, 40, SKYBLUE);
            DrawText("Leaderboard", 650, 400, 40, GOLD);
            
            DrawRectangleRec(backButton, LIGHTGRAY);
            DrawText("Back", backButton.x + 20, backButton.y + 10, 30, BLACK);
            
            if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                if(CheckCollisionPointRec(mousePos, backButton))
                {
                    statsScreen = false;
                    menuScreen = true;
                }
                if(mousePos.x >= 650 && mousePos.x <= 950 && mousePos.y >= 300 && mousePos.y <= 340)
                {
                    statsScreen = false;
                    searchScreen = true;
                    typingActive = true;
                    typedText = "";
                    searchResult = -1;
                }
                if(mousePos.x >= 650 && mousePos.x <= 950 && mousePos.y >= 400 && mousePos.y <= 440)
                {
                    statsScreen = false;
                    leaderboardScreen = true;
                }
            }
        }
        else if(searchScreen)
        {
            DrawText("Search Score", 600, 50, 50, WHITE);
            DrawText("Enter a score:", 150, 200, 35, WHITE);
            
            if(typingActive)
            {
                if(IsKeyPressed(KEY_BACKSPACE) && typedText.length() > 0)
                {
                    typedText.pop_back();
                }
                
                int keyPress = GetCharPressed();
                while(keyPress > 0)
                {
                    if(keyPress >= 48 && keyPress <= 57)
                    {
                        if(typedText.length() < 3)
                            typedText += (char)keyPress;
                    }
                    keyPress = GetCharPressed();
                }
            }
            
            DrawRectangle(150, 270, 300, 50, DARKGRAY);
            DrawText(typedText.c_str(), 160, 280, 30, WHITE);
            
            DrawText("Search", 600, 280, 30, LIME);
            
            if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
               mousePos.x >= 600 && mousePos.x <= 700 && mousePos.y >= 280 && mousePos.y <= 310)
            {
                if(typedText.length() > 0)
                {
                    int scoreVal = stoi(typedText);
                    searchResult = scoreLog.findScore(scoreVal);
                    lastSearch = typedText;
                    typedText = "";
                }
            }
            
            if(IsKeyPressed(KEY_ENTER) && typedText.length() > 0)
            {
                int scoreVal = stoi(typedText);
                searchResult = scoreLog.findScore(scoreVal);
                lastSearch = typedText;
                typedText = "";
            }
            
            if(searchResult > 0)
            {
                DrawText(TextFormat("Score %s found %d times", lastSearch.c_str(), searchResult), 150, 450, 35, YELLOW);
            }
            else if(searchResult == 0 && lastSearch.length() > 0)
            {
                DrawText(TextFormat("Score %s not found", lastSearch.c_str()), 150, 450, 35, RED);
            }
            
            DrawRectangleRec(backButton, LIGHTGRAY);
            DrawText("Back", backButton.x + 20, backButton.y + 10, 30, BLACK);
            
            if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                if(CheckCollisionPointRec(mousePos, backButton))
                {
                    searchScreen = false;
                    statsScreen = true;
                    typedText = "";
                    lastSearch = "";
                    searchResult = -1;
                }
            }
        }
        else if(leaderboardScreen)
        {
            DrawText("Leaderboard", 600, 50, 50, WHITE);
            
            int topScores[5];
            int topFrequencies[5];
            scoreLog.bestFive(topScores, topFrequencies);
            
            int verticalPos = 200;
            for(idx = 0; idx < 5; idx++)
            {
                if(topScores[idx] != -1)
                {
                    DrawText(TextFormat("#%d: Score %d - %d times", idx + 1, topScores[idx], topFrequencies[idx]), 200, verticalPos, 30, WHITE);
                }
                verticalPos += 80;
            }
            
            DrawRectangleRec(backButton, LIGHTGRAY);
            DrawText("Back", backButton.x + 20, backButton.y + 10, 30, BLACK);
            
            if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                if(CheckCollisionPointRec(mousePos, backButton))
                {
                    leaderboardScreen = false;
                    statsScreen = true;
                }
            }
        }
        else if(modeSelect)
        {
            DrawText("Select Difficulty", 600, 150, 50, WHITE);
            DrawText("Easy", 720, 300, 50, LIME);
            DrawText("Medium", 680, 400, 50, ORANGE);
            DrawText("Hard", 720, 500, 50, RED);
            
            DrawRectangleRec(backButton, LIGHTGRAY);
            DrawText("Back", backButton.x + 20, backButton.y + 10, 30, BLACK);
            
            if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                if(CheckCollisionPointRec(mousePos, backButton))
                {
                    modeSelect = false;
                    menuScreen = true;
                }
                else if(mousePos.x >= 650 && mousePos.x <= 950 && mousePos.y >= 300 && mousePos.y <= 350)
                {
                    gameMode = 0;
                    modeSelect = false;
                    countdownScreen = true;
                    countdownBegin = GetTime();
                    hitsTotal = 0;
                    shotsTotal = 0;
                    streak = 0;
                    clearTargets();
                    spawnQueue.prepare(gameMode);
                }
                else if(mousePos.x >= 650 && mousePos.x <= 950 && mousePos.y >= 400 && mousePos.y <= 450)
                {
                    gameMode = 1;
                    modeSelect = false;
                    countdownScreen = true;
                    countdownBegin = GetTime();
                    hitsTotal = 0;
                    shotsTotal = 0;
                    streak = 0;
                    clearTargets();
                    spawnQueue.prepare(gameMode);
                }
                else if(mousePos.x >= 650 && mousePos.x <= 950 && mousePos.y >= 500 && mousePos.y <= 550)
                {
                    gameMode = 2;
                    modeSelect = false;
                    countdownScreen = true;
                    countdownBegin = GetTime();
                    hitsTotal = 0;
                    shotsTotal = 0;
                    streak = 0;
                    clearTargets();
                    spawnQueue.prepare(gameMode);
                }
            }
        }
        else if(countdownScreen)
        {
            double countdownElapsed = GetTime() - countdownBegin;
            
            if(countdownElapsed < 1.0)
            {
                DrawText("3", 780, 400, 120, RED);
            }
            else if(countdownElapsed < 2.0)
            {
                DrawText("2", 780, 400, 120, ORANGE);
            }
            else if(countdownElapsed < 3.0)
            {
                DrawText("1", 780, 400, 120, YELLOW);
            }
            else if(countdownElapsed < 4.0)
            {
                DrawText("GO!", 740, 400, 120, GREEN);
            }
            else
            {
                countdownScreen = false;
                gameActive = true;
                roundStartTime = GetTime();
            }
        }
        else if(gameActive)
        {
            timeElapsed = GetTime() - roundStartTime;
            
            createTarget();
            
            for(idx = 0; idx < activeTargets; idx++)
            {
                if(gameTargets[idx].alive)
                {
                    drawAimTarget(gameTargets[idx].posX, gameTargets[idx].posY, gameTargets[idx].size, gameTargets[idx].shade);
                }
            }
            
            if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                float mouseX = GetMouseX();
                float mouseY = GetMouseY();
                double clickTime = GetTime();
                shotsTotal += 1;
                
                bool hitTarget = false;
                double accuracy = 0.0;
                
                for(idx = 0; idx < activeTargets; idx++)
                {
                    if(gameTargets[idx].alive)
                    {
                        if(calculateDistance(mouseX, mouseY, gameTargets[idx].posX, gameTargets[idx].posY) <= gameTargets[idx].size)
                        {
                            gameTargets[idx].alive = false;
                            hitTarget = true;
                            accuracy = calculatePrecision(gameTargets[idx], mouseX, mouseY);
                            
                            if(hitsTotal < MAX_TARGETS)
                            {
                                speedValues[hitsTotal] = clickTime - gameTargets[idx].created;
                                precisionValues[hitsTotal] = accuracy;
                                hitsTotal += 1;
                            }
                            createAnimation(mouseX, mouseY);
                            break;
                        }
                    }
                }
                
                if(hitTarget && accuracy >= 1.0)
                {
                    streak += 1;
                    if(streak > bestStreak)
                    {
                        bestStreak = streak;
                    }
                }
                else
                {
                    streak = 0;
                }
            }
            
            updateAnimations();
            
            DrawRectangle(0, 0, 400, 150, DARKGRAY);
            DrawText(TextFormat("Time Left: %.1f", roundTime - timeElapsed), 10, 10, 25, WHITE);
            DrawText(TextFormat("Hits: %d", hitsTotal), 10, 45, 25, WHITE);
            DrawText(TextFormat("Shot Accuracy: %.1f%%", shotsTotal ? hitsTotal * 100.0 / shotsTotal : 0), 10, 80, 25, WHITE);
            
            double averageAccuracy = 0;
            for(idx = 0; idx < hitsTotal; idx++)
            {
                averageAccuracy += precisionValues[idx];
            }
            if(hitsTotal > 0) averageAccuracy /= hitsTotal;
            DrawText(TextFormat("Target Accuracy: %.1f%%", averageAccuracy * 100), 10, 115, 25, WHITE);
            
            if(streak >= 2)
            {
                DrawText(TextFormat("%dx Streak!!", streak), 1320, 20, 50, GOLD);
            }
            
            if(timeElapsed >= roundTime)
            {
                double averageSpeed = 0;
                for(idx = 0; idx < hitsTotal; idx++) averageSpeed += speedValues[idx];
                if(hitsTotal > 0) averageSpeed /= hitsTotal;
                
                string currentTime = getCurrentTime();
                sessionLog.addSession(hitsTotal, shotsTotal, averageSpeed, averageAccuracy, currentTime);
                scoreLog.storeScore(hitsTotal);
                
                gameActive = false;
                gameOverScreen = true;
                gameOverBegin = GetTime();
            }
        }
        else if(gameOverScreen)
        {
            DrawText("TIME UP!", 650, 400, 80, RED);
            
            if(GetTime() - gameOverBegin >= 2.0)
            {
                gameOverScreen = false;
                menuScreen = true;
            }
        }
        
        EndDrawing();
    }
    
    sessionLog.saveSessions();
    saveBestStreak();
    
    double saveBegin = GetTime();
    while(GetTime() - saveBegin < 1.0)
    {
        BeginDrawing();
        ClearBackground(BLACK);
        DrawText("Saving Files...", 600, 400, 50, WHITE);
        EndDrawing();
    }
    
    CloseWindow();
    
    return 0;
}