#include <iostream>
#include <vector>
#include <deque>
#include <string>
#include <ctime>
#include <conio.h> 
#include <graphics.h> 
#include <windows.h> 

using namespace std;

// --- 全局常量定义 ---


const int windowWidth = 1440; // 增大窗口宽度
const int windowHeight = 720; // 增大窗口高度
const int gridSize = 40;      // 增大网格大小
const int mapWidth = (windowWidth - 240) / gridSize;
const int mapHeight = (windowHeight) / gridSize; // 预留UI区域


// --- 枚举类型定义 ---
enum Direction { stop = 0, up, down, left_dir, right_dir };
enum GameState { menu = 0, singleMode, pairMode, viewRecords, gameOver };
enum MapElement { emptyInfo = 0, food, snake1Body, snake2Body, wall };
enum GameVersion { selecting = 0, beginner, advanced, expert };
enum keyStatus { none = 0, p1, p2, func };


// 地图状态数组作为全局变量，方便各类访问
MapElement gameMap[mapWidth][mapHeight];

// 初始化地图状态
void resetGameMap() {
    for (int i = 0; i < mapWidth; ++i) {
        for (int j = 0; j < mapHeight; ++j) {
            gameMap[i][j] = emptyInfo;
        }
    }
}

// 获取随机空位置
pair<int, int> getRandomEmptyPosition() {
    vector<pair<int, int>> emptyPositions;
    for (int i = 0; i < mapWidth; ++i) {
        for (int j = 0; j < mapHeight; ++j) {
            if (gameMap[i][j] == emptyInfo) {
                emptyPositions.push_back(make_pair(i, j));
            }
        }
    }
    if (emptyPositions.empty()) {
        return make_pair(-1, -1); // 无空位置
    }

    int idx = rand() % emptyPositions.size();
    return emptyPositions[idx];
}

// 输入处理类 - 处理键盘和鼠标输入
class inputHandler {
private:
    // 标记左键是否按
    bool leftButtonDown;
    // 按键缓冲区，主要是防止玩家在一帧内按下多个键导致的某些键没有识别到
    deque<pair<int, keyStatus>> keyBuffer;

public:
    inputHandler() : leftButtonDown(false) {
    }

    // 更新输入状态
    void updateMouse() {
        // 检查是否有鼠标消息
        if (MouseHit()) {
            MOUSEMSG msg = GetMouseMsg();
            if (msg.uMsg == WM_LBUTTONDOWN) {
                leftButtonDown = true;
            }
            else {
                leftButtonDown = false;
            }
        }
        else {
            leftButtonDown = false;
        }
    }

    // 是否左键按下
    bool isLeftButtonDown() const {
        return leftButtonDown;
    }

    // 获取鼠标实时位置
    pair<int, int> getMousePos() const {
        POINT pt;
        GetCursorPos(&pt);
        // 将屏幕坐标转换为窗口坐标
        ScreenToClient(GetHWnd(), &pt);
        return make_pair(pt.x, pt.y);
    }

    // 检查鼠标是否悬停在矩形内，用于鼠标悬停在按钮上的检测
    bool isMouseHoveringInRect(int x1, int y1, int x2, int y2) const {
        auto pos = getMousePos();
        return (pos.first >= x1 && pos.first <= x2 && pos.second >= y1 && pos.second <= y2);
    }

    void clearKeyBuffer() {
        keyBuffer.clear();
    }

    // 从系统读取所有可用按键，追加到缓冲区尾部
    void fetchNewKeys() {
        while (_kbhit()) {
            int key = _getch();
            switch (key) {
            case 'q': case 'Q':case 'p': case 'P':case 27: case 'r':case 'R':
                // 直接加入缓冲区
                keyBuffer.push_back(make_pair(key, func));
                continue;
            }

            switch (key) {
            case 'W': case 'w':case 'A': case 'a':case 'S': case 's':case 'D': case 'd':
                // 直接加入缓冲区
                keyBuffer.push_back(make_pair(key, p1));
                continue;
            }

            if (key == 0xE0) {
                // 扩展键：再读一次
                if (_kbhit()) {
                    int ext = _getch();
                    switch (ext) {
                    case 72: key = VK_UP; break;
                    case 80: key = VK_DOWN; break;
                    case 75: key = VK_LEFT; break;
                    case 77: key = VK_RIGHT; break;
                    default: key = 0; // 未知扩展键
                    }
                }
                else {
                    key = 0; // 不完整扩展键序列
                }
            }
            else {
                key = 0; // 非扩展键，忽略
            }
            // 此时一定为p2的键
            if (key != 0) {
                keyBuffer.push_back(make_pair(key, p2));
            }
        }
    }

    // 获取第一个指定类型的按键
    pair<int, keyStatus> getKey(keyStatus keyType) {
        // 先获取新按键
        fetchNewKeys();

        for (size_t i = 0; i < keyBuffer.size(); ++i) {
            if (keyBuffer[i].second == keyType) {
                auto key = keyBuffer[i];
                // 只移除该键
                keyBuffer.erase(keyBuffer.begin() + i);
                return key;
            }
        }
        return make_pair(-1, none); // 未找到
    }
};

//蛇类
class Snake {
private:
    // 使用双端队列方便头尾操作
    deque<pair<int, int>> body;
    // 贪吃蛇的移动方向
    Direction dir;
    // 蛇的颜色
    int color;
    // 玩家1 or 2
    int playerId;
    // 蛇的生命值
    int life;
    //是否吃到食物
    bool isEat;
    // 蛇头贴图
    IMAGE imgHead;

    // 加速相关的属性如下
    // 由于我们使用帧数来控制移动速度，从而能在双人模式下实现一只蛇加速而另一只不加速
    // 因此这里的加速是通过改变移动间隔帧数来实现的

    // 是否处于加速状态
    bool isSpeedUp;
    // 加速持续时间（以帧为单位）
    int speedUpDuration;
    // 标记加速开始的时间（以帧为单位）
    int markTime;
    // 每帧递减，到0才移动
    int moveCounter;
    // 每隔多少帧移动一次
    int moveInterval;

public:
    Snake(int playerID)
        : playerId(playerID), life(1), isEat(false), dir(stop),
        speedUpDuration(10), markTime(-100), isSpeedUp(false), moveCounter(0), moveInterval(9) {
        // 在构造函数中加载蛇头贴图，并设置对应的颜色
        if (playerId == 1) {
            if (loadimage(&imgHead, _T("pictures/snake_head1.png"))) {
                cerr << "Failed to load snake_head1.png" << endl;
            }
            color = RED;
        }
        else {
            if (loadimage(&imgHead, _T("pictures/snake_head2.png"))) {
                cerr << "Failed to load snake_head2.png" << endl;
            }
            color = BLUE;
        }
    }

    // 计算新头部位置
    pair<int, int> findNewHead() {
        if (body.empty() || life <= 0) {
            // 返回无效位置
            return make_pair(-1, -1);
        }
        else {
            auto head = body.front();
            int newX = head.first;
            int newY = head.second;

            switch (dir) {
            case up:    newY--; break;
            case down:  newY++; break;
            case left_dir:  newX--; break;
            case right_dir: newX++; break;
            case stop: break;
            }

            return make_pair(newX, newY);
        }
    }

    // 蛇移动函数
    void move() {
        if (life <= 0 || body.empty()) {
            return;
        }

        pair<int, int> newHead = findNewHead();

        // 将新头部加入队列
        body.push_front(newHead);

        // 如果没有吃到食物，则移除尾部
        if (gameMap[newHead.first][newHead.second] != food) {
            isEat = false;
            auto tail = body.back();
            gameMap[tail.first][tail.second] = emptyInfo;
            body.pop_back();
        }
        else {
            isEat = true;
        }

        gameMap[newHead.first][newHead.second] = (playerId == 1) ? snake1Body : snake2Body;
    }

    // 改变蛇的移动方向
    void changeDirection(pair<int, keyStatus> key) {
        // 如果蛇已死则不改变方向
        if (life <= 0 || body.empty()) return;

        // 玩家1使用WASD
        if (playerId == 1 && key.second == p1) {
            switch (key.first) {
            case 'W': case 'w': if (dir != down) dir = up; break;
            case 'S': case 's': if (dir != up) dir = down; break;
            case 'A': case 'a': if (dir != right_dir) dir = left_dir; break;
            case 'D': case 'd': if (dir != left_dir) dir = right_dir; break;
            }
        }
        // 玩家2使用箭头键
        else if (playerId == 2 && key.second == p2) {
            switch (key.first) {
            case VK_UP:    if (dir != down) dir = up; break;
            case VK_DOWN:  if (dir != up) dir = down; break;
            case VK_LEFT:  if (dir != right_dir) dir = left_dir; break;
            case VK_RIGHT: if (dir != left_dir) dir = right_dir; break;
            }
        }
    }

    // 检查是否为尾部位置,主要用于newHead可以为当前的尾部位置的情况
    bool isTail(pair<int, int> pos) {
        auto tail = body.back();
        return tail.first == pos.first && tail.second == pos.second;
    }

    // 检查碰撞
    bool checkCollision(pair<int, int> pos) {
        if (pos.first < 0 || pos.first >= mapWidth || pos.second < 0 || pos.second >= mapHeight
            || (gameMap[pos.first][pos.second] == snake1Body && !isTail(pos))
            || (gameMap[pos.first][pos.second] == snake2Body && !isTail(pos))
            || (gameMap[pos.first][pos.second] == wall)) {
            return true;
        }
        return false;
    }

    // 绘制蛇
    void draw() {
        // 如果蛇已死则不绘制
        if (life <= 0 || body.empty()) return;
        // 绘制蛇身
        for (size_t i = 1; i < body.size(); ++i) {
            auto pos = body[i];
            setfillcolor(color);
            solidrectangle(pos.first * gridSize, pos.second * gridSize, (pos.first + 1) * gridSize, (pos.second + 1) * gridSize);
        }

        // 绘制蛇头
        if (!body.empty()) {
            auto headPos = body.front();
            putimage(headPos.first * gridSize, headPos.second * gridSize, &imgHead);
        }
    }

    // 重置蛇的位置和状态，若没有传入初始方向则随机生成一个
    bool reset(int life_, Direction newDir = static_cast<Direction>(rand() % 4 + 1)) {
        body.clear();
        if (life_ < 0) {
            return true;
        }
        if (life_ == 0) {
            life = 0;
            return true;
        }
        auto newPos = getRandomEmptyPosition();

        // 无法重置，地图已满
        if (newPos.first == -1) {
            return false;
        }
        else {
            body.push_back(newPos);
            gameMap[newPos.first][newPos.second] = (playerId == 1) ? snake1Body : snake2Body;
            dir = newDir;
            isEat = false;
            life = life_;
            moveCounter = 0;
            isSpeedUp = false;
            markTime = -100;
            return true;
        }
    }

    // 获取是否吃到食物状态
    bool isEatState() {
        return isEat;
    }

    // 重置吃到食物状态
    void resetEat() {
        isEat = false;
    }

    // 获取蛇身体位置
    const deque<pair<int, int>>& getBody() const {
        return body;
    }

    // 获取蛇头位置
    pair<int, int> getHeadPos() const {
        if (body.empty()) {
            return make_pair(-2, -2); // 返回无效位置
        }
        else {
            return body.front();
        }
    }

    // 获取蛇的生命值
    int getLife() const {
        return life;
    }


    // 获取是否加速状态
    bool getIsSpeedUp() const {
        return isSpeedUp;
    }

    // 每帧调用：更新加速状态
    void updateSpeedState(int gameTime) {
        isSpeedUp = (gameTime < markTime + speedUpDuration);

        moveInterval = isSpeedUp ? 5 : 9;
    }

    // 设置加速状态
    void setSpeedUp(int gameTime) {
        isSpeedUp = true;
        markTime = gameTime;
    }

    // 每帧调用：决定是否该移动
    bool shouldMoveThisFrame() {
        // 死亡了就不移动
        if (life <= 0 || body.empty()) return false;
        moveCounter++;
        if (moveCounter >= moveInterval) {
            moveCounter = 0;
            return true;
        }
        return false;
    }
};

// 食物结构体
struct Food {
    int x, y;
    int ID;
    int foodScore;
    Food(int x_, int y_, int ID_) : x(x_), y(y_), ID(ID_) {
        // 根据ID设置食物分数
        foodScore = ID + 1;
    }
};

// 食物管理器类
class FoodManager {
private:
    vector<Food> foodList;
    vector<IMAGE*> imgFood;
public:
    FoodManager() {
        // 初始化食物列表
        foodList.clear();

        // 加载食物图片
        IMAGE* food1 = new IMAGE, * food2 = new IMAGE, * food3 = new IMAGE, * food4 = new IMAGE, * food5 = new IMAGE;
        loadimage(food1, _T("pictures/food1.png"));
        loadimage(food2, _T("pictures/food2.png"));
        loadimage(food3, _T("pictures/food3.png"));
        loadimage(food4, _T("pictures/food4.png"));
        loadimage(food5, _T("pictures/food5.png"));
        imgFood.push_back(food1);
        imgFood.push_back(food2);
        imgFood.push_back(food3);
        imgFood.push_back(food4);
        imgFood.push_back(food5);
    }

    // 析构函数释放动态内存
    ~FoodManager() {
        for (auto img : imgFood) {
            delete img;
        }
    }

    // 重置食物列表
    void reset() {
        foodList.clear();
    }

    // 生成1~5个食物，并返回是否生成成功
    // 该bool返回值主要用于检测地图是否已满，也就是模式二是否结束
    bool generateFood() {
        int i = rand() % 5 + 1;
        for (int j = 0; j < i; ++j) {
            auto pos = getRandomEmptyPosition();
            if (pos.first != -1) {
                foodList.push_back(Food(pos.first, pos.second, rand() % 5));
                gameMap[pos.first][pos.second] = food;
            }
            else {
                return false;
            }
        }
        return true;
    }

    // 将蛇的尸体变为食物
    void addFood(const deque<pair<int, int>>& body) {
        for (const auto& pos : body) {
            foodList.push_back(Food(pos.first, pos.second, (rand() % 5)));
            gameMap[pos.first][pos.second] = food;
        }
    }

    // 绘制所有食物
    void draw() {
        for (const auto& f : foodList) {
            if (f.ID >= 0 && f.ID < (int)imgFood.size()
                && imgFood[f.ID] != nullptr && imgFood[f.ID]->getwidth() > 0) {
                putimage(f.x * gridSize, f.y * gridSize, imgFood[f.ID]);
            }
            else {
                // fallback：画绿色圆
                setfillcolor(GREEN);
                solidcircle(f.x * gridSize + gridSize / 2,
                    f.y * gridSize + gridSize / 2,
                    gridSize / 2);
            }
        }
    }

    // 移除指定位置的食物
    void removeFood(pair<int, int> pos) {
        foodList.erase(
            remove_if(foodList.begin(), foodList.end(),
                [pos](const Food& f) {
                    return f.x == pos.first && f.y == pos.second;
                }),
            foodList.end());
    }

    // 检查食物列表是否为空
    bool foodEmpty() {
        return foodList.empty();
    }

    // 获取指定位置食物的分数
    int getScore(pair<int, int> pos) {
        for (const auto& f : foodList) {
            if (f.x == pos.first && f.y == pos.second) {
                return f.foodScore;
            }
        }
        return 0;
    }
};

// 墙管理器类
class WallManager {
private:
    vector<pair<int, int>> wallList;

public:
    WallManager() {
        reset();
    }

    // 重置墙壁位置
    void reset() {
        wallList.clear();
        for (int i = 0; i < mapWidth; ++i) {
            wallList.push_back(make_pair(i, 0));
            wallList.push_back(make_pair(i, mapHeight - 1));
            gameMap[i][0] = wall;
            gameMap[i][mapHeight - 1] = wall;
        }
        for (int j = 0; j < mapHeight; ++j) {
            wallList.push_back(make_pair(0, j));
            wallList.push_back(make_pair(mapWidth - 1, j));
            gameMap[0][j] = wall;
            gameMap[mapWidth - 1][j] = wall;
        }
    }

    // 添加墙壁
    void addWall(const deque<pair<int, int>>& body) {
        for (const auto& pos : body) {
            wallList.push_back(pos);
            gameMap[pos.first][pos.second] = wall;
        }
    }

    // 绘制墙壁
    void drawWalls() {
        for (const auto& wall : wallList) {
            int x = wall.first;
            int y = wall.second;
            setfillcolor(BLACK);
            solidrectangle(x * gridSize, y * gridSize, (x + 1) * gridSize, (y + 1) * gridSize);
        }
    }
};

// 记录信息结构体
struct info {
    int timeTaken;
    int score1, score2;

    // 同时适用于单人和双人模式的构造函数
    info(int timeTaken_ = 0, int score_1 = 0, int score2_ = -1) : score1(score_1), score2(score2_), timeTaken(timeTaken_) {
    }
};

// 记录管理类
class RecordManager {
private:
    info singleBestScore[3];
    info pairBestScore[3];
public:
    // 构造函数初始化记录
    RecordManager() {
        for (int i = 0; i < 3; ++i) {
            singleBestScore[i] = info(0, 0);
            pairBestScore[i] = info(0, 0, 0);
        }
    }

    // 更新单人模式记录
    void updateSingleInfo(int mode, info newInfo) {
        if (newInfo.score1 > singleBestScore[mode].score1) {
            singleBestScore[mode] = newInfo;
        }
    }

    // 更新双人模式记录
    void updatePairInfo(int mode, info newInfo) {
        int totalScore = newInfo.score1 + newInfo.score2;
        int bestTotalScore = pairBestScore[mode].score1 + pairBestScore[mode].score2;
        if (totalScore > bestTotalScore) {
            pairBestScore[mode] = newInfo;
        }
    }

    // 获取单人模式记录
    info getSingleBestInfo(int mode) {
        return singleBestScore[mode];
    }

    // 获取双人模式记录
    info getPairBestInfo(int mode) {
        return pairBestScore[mode];
    }
};

// 游戏主逻辑类
class Game {
private:
    // 地图的各种元素
    Snake snake1, snake2;
    FoodManager foodManager;
    WallManager wallManager;

    // 游戏时间管理
    int gameTime;
    clock_t startTime;

    // 游戏状态管理
    pair<GameState, GameVersion> lastState, state;

    // 输入处理器
    inputHandler inputHandler;

    // 游戏历史记录管理
    int score1, score2;
    int winner;
    RecordManager recordManager;

    // 背景图片
    vector<IMAGE*> imgBackgrounds;

public:
    // 构造函数
    Game() : score1(0), score2(0), gameTime(0), startTime(0),
        snake1(1), snake2(2), winner(0) {
        state = lastState = make_pair(menu, selecting);
        resetGameMap();
        IMAGE* menuBg = new IMAGE;
        IMAGE* menuBg1 = new IMAGE; IMAGE* menuBg2 = new IMAGE; IMAGE* menuBg3 = new IMAGE; IMAGE* menuBg4 = new IMAGE;

        IMAGE* selectBg1 = new IMAGE; IMAGE* selectBg2 = new IMAGE; IMAGE* selectBg3 = new IMAGE; IMAGE* selectBg4 = new IMAGE;

        loadimage(menuBg, _T("pictures/menuBg.png"));
        loadimage(menuBg1, _T("pictures/menuBg1.png"));
        loadimage(menuBg2, _T("pictures/menuBg2.png"));
        loadimage(menuBg3, _T("pictures/menuBg3.png"));
        loadimage(menuBg4, _T("pictures/menuBg4.png"));
        loadimage(selectBg1, _T("pictures/selectBg1.png"));
        loadimage(selectBg2, _T("pictures/selectBg2.png"));
        loadimage(selectBg3, _T("pictures/selectBg3.png"));
        loadimage(selectBg4, _T("pictures/selectBg4.png"));
        imgBackgrounds.push_back(menuBg);
        imgBackgrounds.push_back(menuBg1);
        imgBackgrounds.push_back(menuBg2);
        imgBackgrounds.push_back(menuBg3);
        imgBackgrounds.push_back(menuBg4);
        imgBackgrounds.push_back(selectBg1);
        imgBackgrounds.push_back(selectBg2);
        imgBackgrounds.push_back(selectBg3);
        imgBackgrounds.push_back(selectBg4);
    }

    // 游戏主循环
    void run() {
        while (true) {
            inputHandler.clearKeyBuffer();
            switch (state.first) {
            case menu:
                handleMenu();
                break;
            case viewRecords:
                handleViewRecords();
                break;
            case singleMode:
            case pairMode:
                if (state.second == selecting) {
                    handleSelectVersion();
                }
                else {
                    init();
                    while ((state.first == singleMode || state.first == pairMode)
                        && state.second != selecting) {
                        update();
                        render();
                        Sleep(10);
                    }
                }
                break;

            case gameOver:
                handleGameOver();
                break;
            default:
                exit(1);
            }
        }
    }

    // 菜单处理函数
    void handleMenu() {
        // 按钮尺寸和位置
        const int btnW = 400;
        const int btnH = 100;
        const int centerX = windowWidth * 3 / 4;
        const int startY = 200;

        while (state.first == menu) {
            inputHandler.clearKeyBuffer();
            inputHandler.updateMouse();
            BeginBatchDraw();
            cleardevice();

            putimage(0, 0, imgBackgrounds[0]);

            // 标题（也可用文本框）
            settextstyle(48, 0, _T("SimHei"));
            settextcolor(RED);
            outtextxy(centerX - 200, 80, _T("欢迎来到贪吃荣耀"));

            const TCHAR* labels[] = {
                _T("让我独享经验！"),
                _T("魔丸 & 灵珠"),
                _T("历史最大胃袋"),
                _T("下线吃焖子")
            };

            int y = startY;
            for (int i = 0; i < 4; ++i) {
                int x = centerX - btnW / 2;
                COLORREF textCol = BLACK;

                // 悬停高亮
                if (inputHandler.isMouseHoveringInRect(x, y, x + btnW, y + btnH)) {
                    textCol = RED;    // 悬停文字色
                    clearrectangle(0, 0, 720, 720);
                    putimage(0, 0, imgBackgrounds[1 + i % 4]); // 更换背景
                }

                drawTextWithBackground(labels[i], x, y, btnW, btnH, textCol);

                // 点击检测（仍用矩形区域）
                if (inputHandler.isLeftButtonDown() &&
                    inputHandler.isMouseHoveringInRect(x, y, x + btnW, y + btnH)) {
                    if (i == 0) {
                        state = make_pair(singleMode, selecting);
                    }
                    else if (i == 1) {
                        state = make_pair(pairMode, selecting);
                    }
                    else if (i == 2) {
                        state = make_pair(viewRecords, selecting);
                    }
                    else if (i == 3) {
                        exit(0);
                    }
                }

                y += 120;
            }

            EndBatchDraw();
            Sleep(10);
        }
    }

    // 版本选择处理函数
    void handleSelectVersion() {
        // 按钮尺寸和位置
        const int btnW = 400;
        const int btnH = 100;
        const int centerX = windowWidth * 3 / 4;
        const int startY = 200;

        while (state.second == selecting && (state.first == singleMode || state.first == pairMode)) {
            inputHandler.clearKeyBuffer();
            inputHandler.updateMouse();
            BeginBatchDraw();

            cleardevice();

            if (state.first == singleMode) {
                putimage(0, 0, imgBackgrounds[1]);
            }
            else {
                putimage(0, 0, imgBackgrounds[2]);
            }

            // 标题
            settextstyle(48, 0, _T("SimHei"));
            settextcolor(RED);
            outtextxy(centerX - 200, 80, _T("请选择你的对局难度"));

            const TCHAR* labels[] = {
                _T("金牌战士(入门版)"),
                _T("金牌射手(进阶版)"),
                _T("金牌坦克(高级版)"),
                _T("返回")
            };

            int y = startY;
            for (int i = 0; i < 4; ++i) {
                int x = centerX - btnW / 2;
                COLORREF textCol = BLACK;

                // 悬停高亮
                if (inputHandler.isMouseHoveringInRect(x, y, x + btnW, y + btnH)) {
                    textCol = RED;    // 悬停文字色
                    if (i != 3) {
                        clearrectangle(0, 0, 720, 720);
                        putimage(0, 0, imgBackgrounds[5 + i]); // 更换背景
                    }
                }

                drawTextWithBackground(labels[i], x, y, btnW, btnH, textCol);

                // 点击检测（仍用矩形区域）
                if (inputHandler.isLeftButtonDown() &&
                    inputHandler.isMouseHoveringInRect(x, y, x + btnW, y + btnH)) {
                    if (i == 0) { state.second = beginner; }
                    else if (i == 1) { state.second = advanced; }
                    else if (i == 2) { state.second = expert; }
                    else if (i == 3) {
                        state = make_pair(menu, selecting);
                        return;
                    }
                }

                y += 120; // 增大间距
            }

            // 退出按键检测
            auto key = inputHandler.getKey(func);
            if (key.first == 'q' || key.first == 'Q' || key.first == 27) {
                state = make_pair(menu, selecting);
            }

            EndBatchDraw();
            Sleep(10);
        }
    }

    // 查看记录处理函数
    void handleViewRecords() {
        // 按钮尺寸和位置
        const int btnW = 600;
        const int btnH = 100;
        const int spacingY = 80;
        const int startX = windowWidth / 2 - btnW / 2;
        const int startY = 100;

        while (state.first == viewRecords) {
            inputHandler.clearKeyBuffer();
            inputHandler.updateMouse();
            BeginBatchDraw();

            // 清屏 + 绘制背景
            cleardevice();

            // 标题
            settextstyle(50, 0, _T("SimHei"));
            settextcolor(RED);
            outtextxy(startX + 150, 30, _T("历史最大胃袋"));

            // 定义版本名称（中文）
            const TCHAR* versionNames[2][3] = {
                {_T("让我独享经验·金牌战士(入门版)"), _T("让我独享经验·金牌射手(进阶版)"), _T("让我独享经验·金牌坦克(高级版)")},
                {_T("灵珠 & 魔丸·金牌战士(入门版)"), _T("灵珠 & 魔丸·金牌射手(进阶版)"), _T("灵珠 & 魔丸·金牌坦克(高级版)")}
            };

            for (int i = 0; i < 3; ++i) {
                // 单人记录
                info singleInfo = recordManager.getSingleBestInfo(i);
                TCHAR singleRecordText[256];
                _stprintf_s(singleRecordText, 256, _T("%s\n最高分: %d 分\n时间: %d 秒"),
                    versionNames[0][i], singleInfo.score1, singleInfo.timeTaken);
                drawTextWithBackground(singleRecordText, startX - 400, startY + i * 2 * spacingY, btnW, btnH);

                // 双人记录
                info pairInfo = recordManager.getPairBestInfo(i);
                TCHAR pairRecordText[256];
                _stprintf_s(pairRecordText, 256, _T("%s\n最高分: %d 分\n时间: %d 秒"),
                    versionNames[1][i], pairInfo.score1 + pairInfo.score2, pairInfo.timeTaken);
                drawTextWithBackground(pairRecordText, startX + 400, startY + i * 2 * spacingY, btnW, btnH);
            }

            int returnBtnX = startX;
            int returnBtnY = startY + 6 * spacingY;
            drawTextWithBackground(_T("返 回"), returnBtnX, returnBtnY, btnW, btnH, BLACK, WHITE, RED, 3, 15);

            if (inputHandler.isMouseHoveringInRect(returnBtnX, returnBtnY, returnBtnX + btnW, returnBtnY + btnH)) {
                drawTextWithBackground(_T("返 回"), returnBtnX, returnBtnY, btnW, btnH, RED, WHITE, RED, 3, 15);
            }

            EndBatchDraw();

            // 点击检测
            if (inputHandler.isLeftButtonDown()) {
                // 检测返回按钮
                if (inputHandler.isMouseHoveringInRect(returnBtnX, returnBtnY, returnBtnX + btnW, returnBtnY + btnH)) {
                    state = make_pair(menu, selecting);
                }
            }
            auto key = inputHandler.getKey(func);
            if (key.first == 'q' || key.first == 'Q' || key.first == 27) {
                state = make_pair(menu, selecting);
            }
            Sleep(10);
        }
    }

    // 暂停处理函数
    void handlePause() {
        // 按钮尺寸和位置
        const int btnW = 400;
        const int btnH = 200;
        const int spacingY = 150;
        const int startX = windowWidth / 2 - btnW / 2;
        const int startY = 400;

        bool isPaused = true;

        while (isPaused) {
            inputHandler.clearKeyBuffer();
            inputHandler.updateMouse();
            BeginBatchDraw();

            // 清屏
            cleardevice();

            // 绘制文字
            settextstyle(50, 0, _T("SimHei"));
            drawTextWithBackground(_T("按 P 回到餐桌\n按 Q 曼巴OUT"), startX - 200, startY - 250, btnW + 400, btnH - 50, RED, WHITE, BLUE, 3, 20);
            drawTextWithBackground(_T("回到餐桌"), startX - 200, startY, btnW, btnH, BLACK, WHITE, BLUE, 3, 15);
            drawTextWithBackground(_T("曼巴OUT"), startX + 200, startY, btnW, btnH, BLACK, WHITE, BLUE, 3, 15);

            if (inputHandler.isMouseHoveringInRect(startX - 200, startY, startX + btnW - 200, startY + btnH)) {
                drawTextWithBackground(_T("回到餐桌"), startX - 200, startY, btnW, btnH, RED, WHITE, BLUE, 3, 15);
            }
            else if (inputHandler.isMouseHoveringInRect(startX + 200, startY, startX + btnW + 200, startY + btnH)) {
                drawTextWithBackground(_T("曼巴OUT"), startX + 200, startY, btnW, btnH, RED, WHITE, BLUE, 3, 15);
            }

            EndBatchDraw();

            // 点击检测
            if (inputHandler.isLeftButtonDown()) {
                // 检测返回按钮
                if (inputHandler.isMouseHoveringInRect(startX - 200, startY, startX + btnW - 200, startY + btnH)) {
                    state = lastState;
                    return;
                }
                else if (inputHandler.isMouseHoveringInRect(startX + 200, startY, startX + btnW + 200, startY + btnH)) {
                    state.first = gameOver;
                    return;
                }
            }

            // 键盘检测
            auto pauseKey = inputHandler.getKey(func);
            if (pauseKey.first == 'p' || pauseKey.first == 'P') {
                inputHandler.clearKeyBuffer();
                isPaused = false;
                return;
            }
            else if (pauseKey.first == 'q' || pauseKey.first == 'Q' || pauseKey.first == 27) { // 在暂停时也处理 Q
                inputHandler.clearKeyBuffer();
                state.first = gameOver;
                return;
            }
            Sleep(10);
        }
    }

    // 游戏结束处理函数
    void handleGameOver() {
        // 先更新历史记录
        if (lastState.first == singleMode) {
            recordManager.updateSingleInfo(lastState.second - beginner, info(gameTime, score1));
        }
        else if (lastState.first == pairMode) {
            recordManager.updatePairInfo(lastState.second - beginner, info(gameTime, score1, score2));
        }

        // 按钮尺寸和位置
        const int btnW = 400;
        const int btnH = 200;
        const int spacingY = 150;
        const int startX = windowWidth / 2 - btnW / 2;
        const int startY = 400;

        while (state.first == gameOver) {
            inputHandler.clearKeyBuffer();
            inputHandler.updateMouse();
            BeginBatchDraw();

            // 清屏
            cleardevice();

            // 绘制文字
            settextstyle(50, 0, _T("SimHei"));

            TCHAR gameOverText[256];
            if (lastState.first == singleMode) {
                _stprintf_s(gameOverText, 256, _T("让我独享经验！\n增重: %d kg\n时间: %d 秒"),
                    score1, gameTime);
            }
            else if (lastState.first == pairMode) {
                _stprintf_s(gameOverText, 256,
                    _T("魔丸 & 灵珠\n魔丸增重: %d kg\n灵珠增重: %d kg\n总增重: %d kg\n时间: %d 秒"),
                    score1, score2, score1 + score2, gameTime);

            }
            drawTextWithBackground(gameOverText, startX - 200, startY - 250, btnW + 400, btnH, RED, WHITE, BLUE, 3, 20);
            drawTextWithBackground(_T("再来一顿"), startX - 200, startY, btnW, btnH, BLACK, WHITE, BLUE, 3, 15);
            drawTextWithBackground(_T("曼巴OUT"), startX + 200, startY, btnW, btnH, BLACK, WHITE, BLUE, 3, 15);

            if (inputHandler.isMouseHoveringInRect(startX - 200, startY, startX + btnW - 200, startY + btnH)) {
                drawTextWithBackground(_T("再来一顿"), startX - 200, startY, btnW, btnH, RED, WHITE, BLUE, 3, 15);
            }
            else if (inputHandler.isMouseHoveringInRect(startX + 200, startY, startX + btnW + 200, startY + btnH)) {
                drawTextWithBackground(_T("曼巴OUT"), startX + 200, startY, btnW, btnH, RED, WHITE, BLUE, 3, 15);
            }

            EndBatchDraw();

            // 点击检测
            if (inputHandler.isLeftButtonDown()) {
                // 检测返回按钮
                if (inputHandler.isMouseHoveringInRect(startX - 200, startY, startX + btnW - 200, startY + btnH)) {
                    state = make_pair(lastState.first, lastState.second);
                    init();
                    return;
                }
                else if (inputHandler.isMouseHoveringInRect(startX + 200, startY, startX + btnW + 200, startY + btnH)) {
                    state = make_pair(lastState.first, selecting);
                    return;
                }
            }

            // 键盘检测
            auto pauseKey = inputHandler.getKey(func);
            if (pauseKey.first == 'r' || pauseKey.first == 'R') {
                inputHandler.clearKeyBuffer();
                state = make_pair(lastState.first, lastState.second);
                return;
            }
            else if (pauseKey.first == 'q' || pauseKey.first == 'Q' || pauseKey.first == 27) { // 在暂停时也处理 Q
                inputHandler.clearKeyBuffer();
                state = make_pair(lastState.first, selecting);
                return;
            }
            Sleep(10);
        }
    }

    // 游戏初始化函数
    void init() {
        score1 = score2 = 0;
        gameTime = 0;
        startTime = clock();

        resetGameMap();

        wallManager.reset();

        // 入门版为1条命
        if (state.second == beginner) {
            snake1.reset(1, stop);
            if (state.first == pairMode) {
                snake2.reset(1, stop);
            }
        }
        // 进阶版满命直到剩余空间不足
        else if (state.second == advanced) {
            // 生命值肯定不会超过地图总格子数
            snake1.reset(mapHeight * mapWidth, stop);
            if (state.first == pairMode) {
                snake2.reset(mapHeight * mapWidth, stop);
            }
        }
        // 高级版5条命
        else if (state.second == expert) {
            snake1.reset(5, stop);
            if (state.first == pairMode) {
                snake2.reset(5, stop);
            }
        }

        foodManager.reset();
        foodManager.generateFood();

        inputHandler.clearKeyBuffer();
    }

    // 处理按键输入
    void handleKey(bool move1, bool move2) {
        pair<int, keyStatus> key = inputHandler.getKey(func);
        lastState = state;
        if (key.first == 'p' || key.first == 'P') {
            inputHandler.clearKeyBuffer();
            handlePause();
            return;
        }
        else if (key.first == 27 || key.first == 'q' || key.first == 'Q') {
            inputHandler.clearKeyBuffer();
            state.first = gameOver;
            return;
        }

        // 只有移动时蛇才处理方向键
        if (move1) {
            pair<int, keyStatus> key1 = inputHandler.getKey(p1);
            snake1.changeDirection(key1);
        }

        if (move2 && state.first == pairMode) {
            pair<int, keyStatus> key2 = inputHandler.getKey(p2);
            snake2.changeDirection(key2);
        }
    }

    // 游戏更新函数
    void update() {
        // 如果游戏已结束且处于暂停状态，直接返回
        if (state.first == gameOver) {
            return;
        }

        bool ended = false;
        // 更新游戏时间
        gameTime = (clock() - startTime) / CLOCKS_PER_SEC;

        snake1.updateSpeedState(gameTime);
        if (state.first == pairMode) {
            snake2.updateSpeedState(gameTime);
        }

        // 是否移动
        bool move1 = snake1.shouldMoveThisFrame();
        bool move2 = (state.first == pairMode) ? snake2.shouldMoveThisFrame() : false;

        // 是否死亡标志
        bool die1 = false;
        bool die2 = false;

        // 按键处理
        handleKey(move1, move2);

        auto newhead1 = snake1.findNewHead();
        auto newhead2 = (state.first == pairMode) ? snake2.findNewHead() : make_pair(-1, -1);


        //处理蛇头相撞的特殊情况
        if (move1 || move2) {
            if (state.first == pairMode && (newhead1 == newhead2 || newhead1 == snake2.getHeadPos()
                || newhead2 == snake1.getHeadPos())) {
                // 极特殊情况，两头蛇下一帧到达同一位置，这时候应该还需要移动一次以更新食物状态
                if (newhead1 == newhead2 && newhead1 != snake1.getHeadPos() && newhead2 != snake2.getHeadPos()) {
                    snake1.move();
                    snake2.move();
                }
                if (state.second == beginner || state.second == expert) {
                    // 添加两蛇身体为食物
                    foodManager.addFood(snake1.getBody());
                    foodManager.addFood(snake2.getBody());
                }

                if (state.second == advanced) {
                    // 将蛇变为墙
                    wallManager.addWall(snake1.getBody());
                    wallManager.addWall(snake2.getBody());
                }

                if (!snake1.reset(snake1.getLife() - 1) || !snake2.reset(snake2.getLife() - 1) || !foodManager.generateFood()) {
                    ended = true;
                }
            }
            // 普通碰撞检测
            else {
                // 检查蛇1碰撞
                if (move1) {
                    if (snake1.checkCollision(newhead1)) {
                        if (state.second == advanced) {
                            wallManager.addWall(snake1.getBody());
                        }
                        else {
                            foodManager.addFood(snake1.getBody());
                        }

                        if (!snake1.reset(snake1.getLife() - 1) || !foodManager.generateFood()) {
                            ended = true;
                        }
                    }
                    else {
                        snake1.move();
                    }
                }

                // 检查蛇2碰撞
                if (state.first == pairMode) {
                    if (move2) {
                        if (snake2.checkCollision(newhead2)) {
                            if (state.second == advanced) {
                                wallManager.addWall(snake2.getBody());
                            }
                            else {
                                foodManager.addFood(snake2.getBody());
                            }

                            if (!snake2.reset(snake2.getLife() - 1) || !foodManager.generateFood()) {
                                ended = true;
                            }
                        }
                        else {
                            snake2.move();
                        }
                    }
                }

            }
        }

        // 处理生命值归零结束游戏
        if (state.first == singleMode && snake1.getLife() <= 0) {
            ended = true;
        }
        else if (state.first == pairMode && snake1.getLife() <= 0 && snake2.getLife() <= 0) {
            ended = true;
        }

        // 处理吃食物，特殊食物可加速
        if (snake1.isEatState()) {
            int tempScore = foodManager.getScore(newhead1);
            if (tempScore == 5) {
                snake1.setSpeedUp(gameTime);
            }
            // 加速状态下食物得分翻倍
            score1 += tempScore * (snake1.getIsSpeedUp() ? 2 : 1);
            foodManager.removeFood(newhead1);
            snake1.resetEat();
        }
        if (state.first == pairMode && snake2.isEatState()) {
            int tempScore = foodManager.getScore(newhead2);
            if (tempScore == 5) {
                snake2.setSpeedUp(gameTime);
            }
            // 加速状态下食物得分翻倍
            score2 += tempScore * (snake2.getIsSpeedUp() ? 2 : 1);
            foodManager.removeFood(newhead2);
            snake2.resetEat();
        }

        // 生成新食物（如果没有食物了）
        if (foodManager.foodEmpty()) {
            if (!foodManager.generateFood()) {
                // 地图已满，游戏结束
                ended = true;
            }
        }

        // 游戏结束
        if (ended) {
            lastState = state;
            state.first = gameOver;
            return;
        }
    }

    // 游戏渲染函数
    void render() {
        BeginBatchDraw();
        cleardevice();

        // 1. 绘制游戏地图内容
        wallManager.drawWalls();
        snake1.draw();
        if (state.first == pairMode) {
            snake2.draw();
        }
        foodManager.draw();

        // 2. 计算 UI 区域位置（右侧 200 像素）
        int uiX = mapWidth * gridSize; // UI 起始 X 坐标
        int uiWidth = windowWidth - uiX; // 应为 200
        int panelWidth = uiWidth - 20;  // 留出 10 像素边距
        int panelHeight = 100;
        int spacing = 120;

        // 3. 实时计算游戏时间（秒）
        int currentTime = gameTime;

        // 4. 设置文本样式（确保 drawTextWithBackground 能正确测量）
        settextstyle(24, 0, _T("SimHei"));

        if (state.first == singleMode) {
            // === 单人模式 UI ===
            bool isSpeedUp = snake1.getIsSpeedUp();

            TCHAR timeText[64], scoreText[64];
            _stprintf_s(timeText, _T("贪吃时间:\n%d 秒"), currentTime);
            if (isSpeedUp)
                _stprintf_s(scoreText, _T("魔丸 (WASD):\n%d kg\n是否加速:是"), score1);
            else
                _stprintf_s(scoreText, _T("魔丸 (WASD):\n%d kg\n是否加速:否"), score1);

            // 时间面板
            drawTextWithBackground(
                timeText,
                uiX + 10, 20,
                panelWidth, panelHeight,
                BLACK, WHITE, BLUE, 2, 10
            );

            // 分数面板
            drawTextWithBackground(
                scoreText,
                uiX + 10, 20 + spacing,
                panelWidth, panelHeight,
                BLACK, WHITE, RED, 2, 10
            );

            drawTextWithBackground(
                _T("按 P 暂停\n按 Q 退出"),
                uiX + 10, 20 + 2 * spacing,
                panelWidth, panelHeight,
                BLACK, WHITE, GREEN, 2, 10
            );

        }
        else if (state.first == pairMode) {
            // === 双人模式 UI ===
            bool isSpeedUp1 = snake1.getIsSpeedUp();
            bool isSpeedUp2 = snake2.getIsSpeedUp();

            TCHAR timeText[64], p1Text[64], p2Text[64], totalText[64];
            _stprintf_s(timeText, _T("贪吃时间:\n%d 秒"), currentTime);
            if (isSpeedUp1)
                _stprintf_s(p1Text, _T("魔丸 (WASD):\n%d kg\n是否加速:是"), score1);
            else
                _stprintf_s(p1Text, _T("魔丸 (WASD):\n%d kg\n是否加速:否"), score1);

            if (isSpeedUp2)
                _stprintf_s(p2Text, _T("灵珠 (方向键):\n%d kg\n是否加速:是"), score2);
            else
                _stprintf_s(p2Text, _T("灵珠 (方向键):\n%d kg\n是否加速:否"), score2);
            _stprintf_s(totalText, _T("总胃袋:\n%d kg"), score1 + score2);

            // 时间面板
            drawTextWithBackground(
                timeText,
                uiX + 10, 20,
                panelWidth, panelHeight,
                BLACK, WHITE, BLUE, 2, 10
            );

            // P1 面板
            drawTextWithBackground(
                p1Text,
                uiX + 10, 20 + spacing,
                panelWidth, panelHeight,
                BLACK, WHITE, RED, 2, 10
            );

            // P2 面板
            drawTextWithBackground(
                p2Text,
                uiX + 10, 20 + 2 * spacing,
                panelWidth, panelHeight,
                BLACK, WHITE, BLUE, 2, 10
            );

            // 总分面板
            drawTextWithBackground(
                totalText,
                uiX + 10, 20 + 3 * spacing,
                panelWidth, panelHeight,
                BLACK, WHITE, RGB(255, 20, 147), 2, 10
            );

            // 暂停提示
            drawTextWithBackground(
                _T("按 P 暂停\n按 Q 退出"),
                uiX + 10, 20 + 4 * spacing,
                panelWidth, panelHeight,
                BLACK, WHITE, GREEN, 2, 10
            );
        }

        EndBatchDraw();
    }

    // 辅助函数：按行分割文本
    vector<basic_string<TCHAR>> splitLines(const TCHAR* text) {
        std::vector<std::basic_string<TCHAR>> lines;
        if (!text) return lines;

        std::basic_string<TCHAR> current;
        const TCHAR* p = text;

        while (*p != _T('\0')) {
            if (*p == _T('\n')) {
                lines.push_back(current);
                current.clear();
            }
            else {
                current.push_back(*p);
            }
            p++;
        }
        // 最后一行（即使无 \n）
        lines.push_back(current);

        return lines;
    }

    // 辅助函数：绘制带圆角背景和边框的文本框
    void drawTextWithBackground(
        const TCHAR* text,
        int x, int y,
        int width, int height,
        COLORREF textColor = BLACK,
        COLORREF bgColor = WHITE,
        COLORREF borderColor = BLUE,
        int borderWidth = 5,
        int cornerRadius = 20,
        bool centerVertically = true
    ) {
        if (!text || *text == _T('\0')) return;
        settextstyle(30, 0, _T("SimHei"));

        // === 1. 绘制圆角背景 + 边框 ===
        setfillcolor(borderColor);
        fillroundrect(x, y, x + width, y + height, cornerRadius, cornerRadius);

        setfillcolor(bgColor);
        fillroundrect(
            x + borderWidth, y + borderWidth,
            x + width - borderWidth, y + height - borderWidth,
            cornerRadius - borderWidth, cornerRadius - borderWidth
        );

        // === 2. 分割文本（仅按 '\n' 分割）===
        std::vector<std::basic_string<TCHAR>> lines = splitLines(text);

        // === 3. 计算布局 ===
        int lineHeight = textheight(_T("国")); // 基准行高
        int totalTextHeight = static_cast<int>(lines.size()) * lineHeight;
        int startY = y + borderWidth;
        if (centerVertically) {
            int availableHeight = height - 2 * borderWidth;
            if (totalTextHeight < availableHeight) {
                startY += (availableHeight - totalTextHeight) / 2;
            }
        }

        // === 4. 逐行绘制 ===
        settextcolor(textColor);

        for (size_t i = 0; i < lines.size(); ++i) {
            int lineY = startY + static_cast<int>(i) * lineHeight;
            if (lineY + lineHeight > y + height - borderWidth) break;

            const TCHAR* lineText = lines[i].c_str();
            int textWidth = textwidth(lineText);
            int textX = x + borderWidth + (width - 2 * borderWidth - textWidth) / 2;
            outtextxy(textX, lineY, lineText);
        }
    }
};

// 主函数
int main() {
    srand((unsigned)time(0));
    initgraph(windowWidth, windowHeight);
    setbkcolor(WHITE);
    cleardevice();

    Game game;
    game.run();

    closegraph();
    return 0;
}
