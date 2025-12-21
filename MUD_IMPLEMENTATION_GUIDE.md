# MUD (Multi-User Dungeon) Implementation Guide

## What You're Building

A text-based multiplayer adventure game where players:
- Explore a world of connected rooms
- Fight NPCs (goblins, wolves, etc.)
- Pick up and use items
- Interact with other players in real-time

**Technology:**
- Your Reactor pattern for network I/O
- C++20 coroutines for game logic
- Worker pool for CPU-heavy tasks (pathfinding, combat calculations)

---

## Architecture Overview

```
┌──────────────────────────────────────────────────────────────┐
│                         REACTOR                               │
│  ┌────────────────────────────────────────────────────────┐  │
│  │  GameSession (per connected player)                     │  │
│  │  - Owns Player object                                   │  │
│  │  - Handles commands (coroutines)                        │  │
│  │  - Sends/receives text                                  │  │
│  └────────────────────────────────────────────────────────┘  │
│                           │                                   │
│                           ▼                                   │
│  ┌────────────────────────────────────────────────────────┐  │
│  │                    WORLD (singleton)                    │  │
│  │  - Map of all rooms                                     │  │
│  │  - List of all players                                  │  │
│  │  - Global events (broadcasts, timers)                   │  │
│  └────────────────────────────────────────────────────────┘  │
│                           │                                   │
│                    ┌──────┴──────┐                           │
│                    ▼              ▼                           │
│           ┌──────────────┐  ┌──────────────┐                │
│           │    Room 1    │  │    Room 2    │  ...           │
│           │  - NPCs      │  │  - NPCs      │                │
│           │  - Items     │  │  - Items     │                │
│           │  - Players   │  │  - Players   │                │
│           │  - Exits     │  │  - Exits     │                │
│           └──────────────┘  └──────────────┘                │
└──────────────────────────────────────────────────────────────┘
```

---

## Core Classes

### 1. World - Global Game State

**Purpose:** Central hub for all game objects and state.

**File:** `include/World.hpp`

```cpp
class World {
public:
    static World& instance() {
        static World inst;
        return inst;
    }
    
    // Room management
    Room* getRoom(const std::string& id);
    void addRoom(std::unique_ptr<Room> room);
    
    // Player management
    void addPlayer(Player* player);
    void removePlayer(Player* player);
    std::vector<Player*> getPlayersInRoom(const std::string& roomId);
    
    // Broadcasting
    void broadcastToRoom(const std::string& roomId, const std::string& message);
    void broadcastToAll(const std::string& message);
    
    // World initialization
    void loadWorld();  // Create rooms, NPCs, items
    
private:
    World() = default;
    
    std::unordered_map<std::string, std::unique_ptr<Room>> rooms_;
    std::vector<Player*> players_;  // Non-owning pointers
};
```

**Design Notes:**
- Singleton pattern (one world for all players)
- Owns all rooms
- Non-owning pointers to players (owned by GameSession)
- All access from reactor thread only (no locking needed)

### 2. Room - Game Location

**File:** `include/Room.hpp`

```cpp
class Room {
public:
    Room(std::string id, std::string name, std::string description);
    
    // Accessors
    const std::string& getId() const { return id_; }
    const std::string& getName() const { return name_; }
    const std::string& getDescription() const { return description_; }
    
    // Exits (connections to other rooms)
    void addExit(const std::string& direction, const std::string& targetRoomId);
    const std::string* getExit(const std::string& direction) const;
    std::vector<std::string> getExitDirections() const;
    
    // NPCs
    void addNPC(std::unique_ptr<NPC> npc);
    NPC* findNPC(const std::string& name);
    void removeNPC(const std::string& name);
    std::vector<NPC*> getNPCs();
    
    // Items
    void addItem(std::unique_ptr<Item> item);
    Item* findItem(const std::string& name);
    std::unique_ptr<Item> takeItem(const std::string& name);
    std::vector<Item*> getItems();
    
    // Players (tracked for broadcasting)
    void addPlayer(Player* player);
    void removePlayer(Player* player);
    std::vector<Player*> getPlayers();
    
private:
    std::string id_;
    std::string name_;
    std::string description_;
    
    std::unordered_map<std::string, std::string> exits_;  // direction -> roomId
    std::unordered_map<std::string, std::unique_ptr<NPC>> npcs_;
    std::unordered_map<std::string, std::unique_ptr<Item>> items_;
    std::vector<Player*> players_;  // Non-owning
};
```

**Example Room:**
```
ID: "forest_clearing"
Name: "A Forest Clearing"
Description: "Sunlight filters through the canopy above. Birds chirp in the trees."
Exits: { "north" -> "dark_forest", "east" -> "river_bank" }
NPCs: { "goblin" -> GoblinNPC }
Items: { "sword" -> RustySwordItem }
```

### 3. Player - Player State

**File:** `include/Player.hpp`

```cpp
class Player {
public:
    Player(const std::string& name);
    
    // Basic info
    const std::string& getName() const { return name_; }
    
    // Location
    void setCurrentRoom(Room* room);
    Room* getCurrentRoom() const { return currentRoom_; }
    
    // Stats
    int getHealth() const { return health_; }
    int getMaxHealth() const { return maxHealth_; }
    void takeDamage(int amount);
    void heal(int amount);
    bool isDead() const { return health_ <= 0; }
    
    int getLevel() const { return level_; }
    void gainExperience(int xp);
    
    // Inventory
    void addItem(std::unique_ptr<Item> item);
    Item* findItem(const std::string& name);
    std::unique_ptr<Item> removeItem(const std::string& name);
    std::vector<Item*> getInventory();
    
    // Equipment
    void equipWeapon(Item* weapon);
    void equipArmor(Item* armor);
    Item* getWeapon() const { return weapon_; }
    Item* getArmor() const { return armor_; }
    
    // Combat stats (calculated from equipment + level)
    int getAttackPower() const;
    int getDefense() const;
    
private:
    std::string name_;
    Room* currentRoom_ = nullptr;
    
    // Stats
    int health_ = 100;
    int maxHealth_ = 100;
    int level_ = 1;
    int experience_ = 0;
    
    // Inventory
    std::unordered_map<std::string, std::unique_ptr<Item>> inventory_;
    Item* weapon_ = nullptr;  // Non-owning pointer to item in inventory
    Item* armor_ = nullptr;
};
```

### 4. NPC - Non-Player Character

**File:** `include/NPC.hpp`

```cpp
class NPC {
public:
    NPC(std::string name, std::string description, int health, int level);
    
    const std::string& getName() const { return name_; }
    const std::string& getDescription() const { return description_; }
    
    // Combat
    int getHealth() const { return health_; }
    void takeDamage(int amount);
    bool isDead() const { return health_ <= 0; }
    
    int getLevel() const { return level_; }
    int getAttackPower() const;
    int getDefense() const;
    
    // Loot
    void setLoot(std::vector<std::unique_ptr<Item>> loot);
    std::vector<std::unique_ptr<Item>> dropLoot();
    
private:
    std::string name_;
    std::string description_;
    int health_;
    int maxHealth_;
    int level_;
    std::vector<std::unique_ptr<Item>> loot_;
};
```

### 5. Item - Game Object

**File:** `include/Item.hpp`

```cpp
enum class ItemType {
    WEAPON,
    ARMOR,
    CONSUMABLE,
    QUEST_ITEM
};

class Item {
public:
    Item(std::string name, std::string description, ItemType type);
    
    const std::string& getName() const { return name_; }
    const std::string& getDescription() const { return description_; }
    ItemType getType() const { return type_; }
    
    // For weapons/armor
    void setDamage(int damage) { damage_ = damage; }
    void setDefense(int defense) { defense_ = defense; }
    int getDamage() const { return damage_; }
    int getDefense() const { return defense_; }
    
    // For consumables
    void setHealAmount(int amount) { healAmount_ = amount; }
    int getHealAmount() const { return healAmount_; }
    
private:
    std::string name_;
    std::string description_;
    ItemType type_;
    int damage_ = 0;
    int defense_ = 0;
    int healAmount_ = 0;
};
```

### 6. GameSession - Player Connection Handler

**File:** `include/GameSession.hpp`

```cpp
class GameSession : public EventHandler,
                    public std::enable_shared_from_this<GameSession> {
public:
    GameSession(int fd, Reactor* reactor);
    ~GameSession();
    
    int getHandle() const override { return fd_; }
    void handleRead() override;
    
    // Start the game loop
    void start();
    
private:
    int fd_;
    Reactor* reactor_;
    std::unique_ptr<Player> player_;
    std::string inputBuffer_;
    bool authenticated_ = false;
    
    // Coroutine handlers
    Task<void> gameLoop();
    Task<void> authenticate();
    Task<void> processCommand(std::string command);
    
    // Command handlers (all coroutines)
    Task<void> handleLook();
    Task<void> handleGo(const std::string& direction);
    Task<void> handleTake(const std::string& itemName);
    Task<void> handleDrop(const std::string& itemName);
    Task<void> handleInventory();
    Task<void> handleAttack(const std::string& targetName);
    Task<void> handleSay(const std::string& message);
    Task<void> handleHelp();
    
    // Helper methods
    void send(const std::string& message);
    void sendPrompt();
    std::vector<std::string> parseCommand(const std::string& input);
};
```

---

## Implementation Steps

### Step 1: World Building (1-2 hours)

Create the world with 5-10 interconnected rooms.

```cpp
// In src/World.cpp
void World::loadWorld() {
    // Room 1: Town Square
    auto townSquare = std::make_unique<Room>(
        "town_square",
        "Town Square",
        "A bustling town square with a fountain in the center. "
        "Paths lead north to the forest and east to the market."
    );
    townSquare->addExit("north", "dark_forest");
    townSquare->addExit("east", "market");
    
    // Add a friendly NPC
    auto guard = std::make_unique<NPC>("guard", "A town guard stands watch.", 50, 1);
    townSquare->addNPC(std::move(guard));
    
    addRoom(std::move(townSquare));
    
    // Room 2: Dark Forest
    auto darkForest = std::make_unique<Room>(
        "dark_forest",
        "Dark Forest",
        "The forest is dense and foreboding. Strange sounds echo in the distance."
    );
    darkForest->addExit("south", "town_square");
    darkForest->addExit("north", "forest_clearing");
    
    // Add hostile NPC
    auto goblin = std::make_unique<NPC>("goblin", "A small goblin with a rusty dagger.", 30, 2);
    auto loot = std::make_unique<Item>("dagger", "A rusty dagger.", ItemType::WEAPON);
    loot->setDamage(5);
    std::vector<std::unique_ptr<Item>> goblinLoot;
    goblinLoot.push_back(std::move(loot));
    goblin->setLoot(std::move(goblinLoot));
    darkForest->addNPC(std::move(goblin));
    
    addRoom(std::move(darkForest));
    
    // Room 3: Market
    auto market = std::make_unique<Room>(
        "market",
        "Town Market",
        "Merchants sell their wares under colorful awnings."
    );
    market->addExit("west", "town_square");
    
    // Add items for sale
    auto sword = std::make_unique<Item>("sword", "A sharp iron sword.", ItemType::WEAPON);
    sword->setDamage(10);
    market->addItem(std::move(sword));
    
    auto potion = std::make_unique<Item>("potion", "A healing potion.", ItemType::CONSUMABLE);
    potion->setHealAmount(50);
    market->addItem(std::move(potion));
    
    addRoom(std::move(market));
    
    // ... add more rooms
}
```

### Step 2: Basic Commands (2-3 hours)

Implement fundamental commands to navigate and interact.

#### `look` - Examine current room

```cpp
Task<void> GameSession::handleLook() {
    Room* room = player_->getCurrentRoom();
    
    std::ostringstream msg;
    msg << "\n" << room->getName() << "\n";
    msg << room->getDescription() << "\n\n";
    
    // Show exits
    auto exits = room->getExitDirections();
    if (!exits.empty()) {
        msg << "Exits: ";
        for (size_t i = 0; i < exits.size(); ++i) {
            msg << exits[i];
            if (i < exits.size() - 1) msg << ", ";
        }
        msg << "\n\n";
    }
    
    // Show NPCs
    auto npcs = room->getNPCs();
    if (!npcs.empty()) {
        msg << "You see:\n";
        for (auto* npc : npcs) {
            msg << "  - " << npc->getName() << " (" << npc->getDescription() << ")\n";
        }
        msg << "\n";
    }
    
    // Show items
    auto items = room->getItems();
    if (!items.empty()) {
        msg << "Items here:\n";
        for (auto* item : items) {
            msg << "  - " << item->getName() << "\n";
        }
        msg << "\n";
    }
    
    // Show other players
    auto players = room->getPlayers();
    for (auto* p : players) {
        if (p != player_.get()) {
            msg << "  - " << p->getName() << " is here.\n";
        }
    }
    
    send(msg.str());
    co_return;
}
```

#### `go` - Move between rooms

```cpp
Task<void> GameSession::handleGo(const std::string& direction) {
    Room* currentRoom = player_->getCurrentRoom();
    
    const std::string* targetRoomId = currentRoom->getExit(direction);
    if (!targetRoomId) {
        send("You can't go that way.\n");
        co_return;
    }
    
    Room* targetRoom = World::instance().getRoom(*targetRoomId);
    if (!targetRoom) {
        send("Error: Room not found.\n");
        co_return;
    }
    
    // Notify others in current room
    World::instance().broadcastToRoom(
        currentRoom->getId(),
        player_->getName() + " leaves " + direction + ".\n",
        player_.get()  // Exclude self
    );
    
    // Move player
    currentRoom->removePlayer(player_.get());
    player_->setCurrentRoom(targetRoom);
    targetRoom->addPlayer(player_.get());
    
    // Notify others in new room
    World::instance().broadcastToRoom(
        targetRoom->getId(),
        player_->getName() + " arrives.\n",
        player_.get()
    );
    
    send("You go " + direction + ".\n\n");
    
    // Auto-look at new room
    co_await handleLook();
}
```

#### `attack` - Combat (with coroutines!)

```cpp
struct CombatResult {
    int playerDamage;
    int npcDamage;
    bool playerCrit;
    bool npcCrit;
};

Task<void> GameSession::handleAttack(const std::string& targetName) {
    Room* room = player_->getCurrentRoom();
    NPC* target = room->findNPC(targetName);
    
    if (!target) {
        send("There's no " + targetName + " here.\n");
        co_return;
    }
    
    if (target->isDead()) {
        send("The " + targetName + " is already dead.\n");
        co_return;
    }
    
    send("You attack " + target->getName() + "!\n");
    
    // Broadcast to others
    World::instance().broadcastToRoom(
        room->getId(),
        player_->getName() + " attacks " + target->getName() + "!\n",
        player_.get()
    );
    
    // Offload combat calculation to worker thread
    auto result = co_await asyncWork<CombatResult>(reactor_, [=]() {
        CombatResult res;
        
        // Simulate complex combat calculation (RNG, stats, equipment)
        int playerAttack = player_->getAttackPower();
        int npcDefense = target->getDefense();
        
        res.playerCrit = (rand() % 100) < 15;  // 15% crit chance
        res.playerDamage = std::max(1, playerAttack - npcDefense);
        if (res.playerCrit) res.playerDamage *= 2;
        
        int npcAttack = target->getAttackPower();
        int playerDefense = player_->getDefense();
        
        res.npcCrit = (rand() % 100) < 10;
        res.npcDamage = std::max(1, npcAttack - playerDefense);
        if (res.npcCrit) res.npcDamage *= 2;
        
        // Simulate slow calculation
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        return res;
    });
    
    // Back on reactor thread - apply damage
    target->takeDamage(result.playerDamage);
    
    std::ostringstream msg;
    if (result.playerCrit) msg << "CRITICAL HIT! ";
    msg << "You deal " << result.playerDamage << " damage to " << target->getName() << ".\n";
    
    if (target->isDead()) {
        msg << target->getName() << " is defeated!\n";
        
        // Drop loot
        auto loot = target->dropLoot();
        for (auto& item : loot) {
            msg << "  - " << target->getName() << " dropped: " << item->getName() << "\n";
            room->addItem(std::move(item));
        }
        
        // Remove NPC
        room->removeNPC(target->getName());
        
        // Grant XP
        int xp = target->getLevel() * 10;
        player_->gainExperience(xp);
        msg << "You gain " << xp << " experience.\n";
        
        send(msg.str());
        
        // Broadcast
        World::instance().broadcastToRoom(
            room->getId(),
            player_->getName() + " defeated " + target->getName() + "!\n",
            player_.get()
        );
        
        co_return;
    }
    
    send(msg.str());
    
    // NPC counterattack
    player_->takeDamage(result.npcDamage);
    
    msg.str("");
    msg.clear();
    if (result.npcCrit) msg << "CRITICAL HIT! ";
    msg << target->getName() << " hits you for " << result.npcDamage << " damage!\n";
    msg << "Health: " << player_->getHealth() << "/" << player_->getMaxHealth() << "\n";
    send(msg.str());
    
    if (player_->isDead()) {
        send("You have been defeated!\n");
        
        // Respawn logic
        player_->heal(player_->getMaxHealth());
        
        Room* spawnRoom = World::instance().getRoom("town_square");
        room->removePlayer(player_.get());
        player_->setCurrentRoom(spawnRoom);
        spawnRoom->addPlayer(player_.get());
        
        send("You respawn at the town square.\n\n");
        co_await handleLook();
    }
}
```

### Step 3: Main Game Loop (1 hour)

```cpp
Task<void> GameSession::gameLoop() {
    // Welcome message
    send("\n=== Welcome to the MUD! ===\n\n");
    
    // Authentication (get player name)
    co_await authenticate();
    
    // Spawn in starting room
    Room* startRoom = World::instance().getRoom("town_square");
    player_->setCurrentRoom(startRoom);
    startRoom->addPlayer(player_.get());
    World::instance().addPlayer(player_.get());
    
    // Show initial room
    co_await handleLook();
    sendPrompt();
    
    // Main loop handled by handleRead() + processCommand()
    co_return;
}

Task<void> GameSession::authenticate() {
    send("Enter your name: ");
    
    // Wait for input (this is tricky - see note below)
    // For now, we'll handle this in handleRead()
    
    co_return;
}

Task<void> GameSession::processCommand(std::string input) {
    auto parts = parseCommand(input);
    if (parts.empty()) {
        co_return;
    }
    
    std::string cmd = parts[0];
    
    if (cmd == "look" || cmd == "l") {
        co_await handleLook();
    } else if (cmd == "go" && parts.size() > 1) {
        co_await handleGo(parts[1]);
    } else if (cmd == "north" || cmd == "n") {
        co_await handleGo("north");
    } else if (cmd == "south" || cmd == "s") {
        co_await handleGo("south");
    } else if (cmd == "east" || cmd == "e") {
        co_await handleGo("east");
    } else if (cmd == "west" || cmd == "w") {
        co_await handleGo("west");
    } else if (cmd == "take" && parts.size() > 1) {
        co_await handleTake(parts[1]);
    } else if (cmd == "drop" && parts.size() > 1) {
        co_await handleDrop(parts[1]);
    } else if (cmd == "inventory" || cmd == "i") {
        co_await handleInventory();
    } else if (cmd == "attack" && parts.size() > 1) {
        co_await handleAttack(parts[1]);
    } else if (cmd == "say" && parts.size() > 1) {
        co_await handleSay(input.substr(4));  // Everything after "say "
    } else if (cmd == "help") {
        co_await handleHelp();
    } else if (cmd == "quit") {
        send("Goodbye!\n");
        close(fd_);
    } else {
        send("Unknown command. Type 'help' for commands.\n");
    }
    
    sendPrompt();
    co_return;
}
```

### Step 4: Integration with Reactor (1-2 hours)

Modify `main.cpp` to use `GameSession` instead of `ConnectionHandler`:

```cpp
// In src/main.cpp
int main() {
    Reactor reactor;
    
    // Initialize world
    World::instance().loadWorld();
    
    int serverFd = /* ... create listening socket ... */;
    
    auto acceptor = std::make_shared<AcceptorHandler>(serverFd, &reactor);
    reactor.registerHandler(acceptor);
    
    std::cout << "MUD server listening on port 9000\n";
    
    reactor.eventLoop();
    return 0;
}
```

Modify `AcceptorHandler` to create `GameSession`:

```cpp
// In src/AcceptorHandler.cpp
void AcceptorHandler::handleRead() {
    // ... accept client ...
    
    auto session = std::make_shared<GameSession>(client, reactor_);
    reactor_->registerHandler(session);
    
    // Start the game loop
    reactor_->spawn(session->gameLoop());
}
```

---

## Testing Your MUD

### Test 1: Basic Movement
```
> look
Town Square
A bustling town square...
Exits: north, east

> go north
You go north.

Dark Forest
The forest is dense...
```

### Test 2: Combat
```
> attack goblin
You attack goblin!
You deal 12 damage to goblin.
goblin hits you for 5 damage!
Health: 95/100

> attack goblin
You attack goblin!
CRITICAL HIT! You deal 24 damage to goblin.
goblin is defeated!
  - goblin dropped: dagger
You gain 20 experience.
```

### Test 3: Multiplayer (open two terminals)
```
Terminal 1:
> say Hello world!
You say: Hello world!

Terminal 2:
Player1 says: Hello world!
```

---

## Advanced Features (Future Enhancements)

### 1. Quests
```cpp
class Quest {
    std::string id;
    std::string title;
    std::string description;
    std::vector<QuestObjective> objectives;
    std::vector<std::unique_ptr<Item>> rewards;
};

Task<void> GameSession::handleQuest(const std::string& questId) {
    // Multi-step quest with async objectives
    co_await completeObjective1();
    send("Objective 1 complete!\n");
    
    co_await completeObjective2();
    send("Objective 2 complete!\n");
    
    send("Quest complete! You receive: Iron Sword\n");
    player_->addItem(/* reward */);
}
```

### 2. Timed Events
```cpp
Task<void> spawnRareMonster() {
    co_await sleep(300);  // 5 minutes
    
    Room* room = World::instance().getRoom("dark_forest");
    auto dragon = std::make_unique<NPC>("dragon", "A fearsome dragon!", 200, 10);
    room->addNPC(std::move(dragon));
    
    World::instance().broadcastToAll("A dragon has appeared in the Dark Forest!\n");
}
```

### 3. Persistence (SQLite)
```cpp
Task<void> GameSession::savePlayer() {
    co_await asyncWork<void>(reactor_, [this]() {
        // Save to database in worker thread
        db.execute("UPDATE players SET health=?, x=?, y=? WHERE name=?",
                   player_->getHealth(), player_->getX(), player_->getY(), player_->getName());
    });
}
```

---

## Common Challenges & Solutions

### Challenge 1: Reading User Input Asynchronously

**Problem:** `handleRead()` is event-driven, but coroutines want `co_await readLine()`.

**Solution A:** Use a promise/future pattern:
```cpp
class GameSession {
    std::promise<std::string> inputPromise_;
    
    void handleRead() override {
        // Parse input
        std::string line = /* extract line */;
        inputPromise_.set_value(line);
    }
    
    Task<std::string> readLine() {
        std::future<std::string> fut = inputPromise_.get_future();
        co_return fut.get();  // Blocks! Bad for coroutines
    }
};
```

**Solution B (Better):** Keep event-driven model:
```cpp
void GameSession::handleRead() override {
    // Accumulate input
    char buffer[1024];
    ssize_t n = recv(fd_, buffer, sizeof(buffer), 0);
    inputBuffer_.append(buffer, n);
    
    // Process complete lines
    size_t pos;
    while ((pos = inputBuffer_.find('\n')) != std::string::npos) {
        std::string line = inputBuffer_.substr(0, pos);
        inputBuffer_.erase(0, pos + 1);
        
        // Spawn coroutine to process command
        reactor_->spawn(processCommand(line));
    }
}
```

### Challenge 2: Broadcasting to Multiple Clients

```cpp
void World::broadcastToRoom(const std::string& roomId, const std::string& message, Player* exclude) {
    Room* room = getRoom(roomId);
    if (!room) return;
    
    for (Player* player : room->getPlayers()) {
        if (player != exclude) {
            GameSession* session = player->getSession();  // Need to store this
            session->send(message);
        }
    }
}
```

**Note:** You'll need to store a `GameSession*` pointer in the `Player` object.

### Challenge 3: NPC AI with Coroutines

```cpp
Task<void> goblinAI(NPC* goblin, Room* room) {
    while (!goblin->isDead()) {
        co_await sleep(5);  // Wait 5 seconds
        
        // Random action
        int action = rand() % 3;
        if (action == 0) {
            // Move to random exit
            auto exits = room->getExitDirections();
            if (!exits.empty()) {
                std::string dir = exits[rand() % exits.size()];
                // Move goblin...
            }
        } else if (action == 1) {
            // Attack nearest player
            auto players = room->getPlayers();
            if (!players.empty()) {
                Player* target = players[0];
                // Attack...
            }
        }
    }
}
```

---

## Performance Considerations

### Memory Usage
- Each coroutine allocates ~1KB on heap
- With 1000 players, ~1MB for coroutine frames
- Acceptable for most scenarios

### CPU Usage
- Reactor thread handles all I/O (usually not CPU-bound)
- Workers handle combat/pathfinding (parallelized)
- Typical load: 10-20% CPU for 100 concurrent players

### Scalability
- Single reactor limits ~10K connections (epoll limit)
- For more: multi-reactor pattern (like Nginx)
- Database becomes bottleneck before network

---

## Next Steps

1. **Implement core classes** (World, Room, Player, NPC, Item)
2. **Test basic world** (create 3-5 rooms, test navigation)
3. **Add GameSession** (replace ConnectionHandler)
4. **Implement commands** (look, go, take, attack)
5. **Test multiplayer** (run two clients)
6. **Add polish** (help text, error messages, colors with ANSI codes)
7. **Advanced features** (quests, persistence, AI)

---

## Resources

- **MUD Design:** "Designing Virtual Worlds" by Richard Bartle
- **Networking:** "Unix Network Programming" by Stevens
- **C++ Async:** "C++ Concurrency in Action" by Anthony Williams

---

**Good luck building your MUD!** Remember: start simple, test often, iterate. The coroutine model will make complex game logic much easier to reason about. Have fun! 🎮
