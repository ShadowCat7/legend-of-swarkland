#include "map.hpp"

#include "individual.hpp"
#include "swarkland.hpp"
#include "item.hpp"

int dungeon_level = 0;
MapMatrix<Tile> actual_map_tiles;
Coord stairs_down_location;

static bool is_open_line_of_sight(Coord from_location, Coord to_location) {
    if (from_location == to_location)
        return true;
    Coord abs_delta = {abs(to_location.x - from_location.x), abs(to_location.y - from_location.y)};
    if (abs_delta.y > abs_delta.x) {
        // primarily vertical
        int dy = sign(to_location.y - from_location.y);
        for (int y = from_location.y + dy; y * dy < to_location.y * dy; y += dy) {
            // x = y * m + b
            // m = run / rise
            int x = (y - from_location.y) * (to_location.x - from_location.x) / (to_location.y - from_location.y) + from_location.x;
            if (!is_open_space(actual_map_tiles[Coord{x, y}].tile_type))
                return false;
        }
    } else {
        // primarily horizontal
        int dx = sign(to_location.x - from_location.x);
        for (int x = from_location.x + dx; x * dx < to_location.x * dx; x += dx) {
            // y = x * m + b
            // m = rise / run
            int y = (x - from_location.x) * (to_location.y - from_location.y) / (to_location.x - from_location.x) + from_location.y;
            if (!is_open_space(actual_map_tiles[Coord{x, y}].tile_type))
                return false;
        }
    }
    return true;
}

static void refresh_normal_vision(Thing individual) {
    Coord you_location = individual->location;
    for (Coord target = {0, 0}; target.y < map_size.y; target.y++) {
        for (target.x = 0; target.x < map_size.x; target.x++) {
            if (!is_open_line_of_sight(you_location, target))
                continue;
            individual->life()->knowledge.tile_is_visible[target].normal = true;
            individual->life()->knowledge.tiles[target] = actual_map_tiles[target];
        }
    }
}

static const int ethereal_radius = 5;
static void refresh_ethereal_vision(Thing individual) {
    Coord you_location = individual->location;
    Coord etheral_radius_diagonal = {ethereal_radius, ethereal_radius};
    Coord upper_left = clamp(you_location - etheral_radius_diagonal, Coord{0, 0}, map_size - Coord{1, 1});
    Coord lower_right= clamp(you_location + etheral_radius_diagonal, Coord{0, 0}, map_size - Coord{1, 1});
    for (Coord target = upper_left; target.y <= lower_right.y; target.y++) {
        for (target.x = upper_left.x; target.x <= lower_right.x; target.x++) {
            if (euclidean_distance_squared(target, you_location) > ethereal_radius * ethereal_radius)
                continue;
            individual->life()->knowledge.tile_is_visible[target].ethereal = true;
            individual->life()->knowledge.tiles[target] = actual_map_tiles[target];
        }
    }
}

void compute_vision(Thing observer) {
    // mindless monsters can't remember the terrain
    if (!observer->life()->species()->has_mind)
        observer->life()->knowledge.tiles.set_all(unknown_tile);

    observer->life()->knowledge.tile_is_visible.set_all(VisionTypes::none());
    if (observer->life()->species()->vision_types.normal)
        refresh_normal_vision(observer);
    if (observer->life()->species()->vision_types.ethereal)
        refresh_ethereal_vision(observer);

    // see individuals
    // first clear out anything that we know are no longer where we thought
    List<PerceivedThing> remove_these;
    PerceivedThing target;
    for (auto iterator = observer->life()->knowledge.perceived_things.value_iterator(); iterator.next(&target);) {
        Coord target_location = get_thing_location(observer, target);
        if (target_location == Coord::nowhere())
            continue;
        if (observer->life()->species()->has_mind && !observer->life()->knowledge.tile_is_visible[target_location].any())
            continue;
        remove_these.append(target);
        List<PerceivedThing> inventory;
        find_items_in_inventory(observer, target, &inventory);
        remove_these.append_all(inventory);
    }
    // do this as a second pass, because modifying in the middle of iteration doesn't work properly.
    for (int i = 0; i < remove_these.length(); i++)
        observer->life()->knowledge.perceived_things.remove(remove_these[i]->id);

    // now see anything that's in our line of vision
    Thing actual_target;
    for (auto iterator = actual_things.value_iterator(); iterator.next(&actual_target);) {
        if (!actual_target->still_exists)
            continue;
        PerceivedThing perceived_target = perceive_thing(observer, actual_target);
        if (perceived_target == nullptr)
            continue;
        observer->life()->knowledge.perceived_things.put(perceived_target->id, perceived_target);
    }
}

void generate_map() {
    dungeon_level++;

    // randomize the appearance of every tile, even if it doesn't matter.
    for (Coord cursor = {0, 0}; cursor.y < map_size.y; cursor.y++) {
        for (cursor.x = 0; cursor.x < map_size.x; cursor.x++) {
            Tile & tile = actual_map_tiles[cursor];
            tile.tile_type = TileType_WALL;
            tile.aesthetic_index = random_int(8);
        }
    }
    // line the border with special undiggable walls
    for (int x = 0; x < map_size.x; x++) {
        actual_map_tiles[Coord{x, 0}].tile_type = TileType_BORDER_WALL;
        actual_map_tiles[Coord{x, map_size.y - 1}].tile_type = TileType_BORDER_WALL;
    }
    for (int y = 0; y < map_size.y; y++) {
        actual_map_tiles[Coord{0, y}].tile_type = TileType_BORDER_WALL;
        actual_map_tiles[Coord{map_size.x - 1, y}].tile_type = TileType_BORDER_WALL;
    }

    // create rooms
    List<SDL_Rect> rooms;
    for (int i = 0; i < 50; i++) {
        int width = random_int(4, 10);
        int height = random_int(4, 10);
        int x = random_int(0, map_size.x - width);
        int y = random_int(0, map_size.y - height);
        SDL_Rect room = SDL_Rect{x, y, width, height};
        for (int j = 0; j < rooms.length(); j++) {
            SDL_Rect intersection;
            if (SDL_IntersectRect(&rooms[j], &room, &intersection)) {
                // overlaps with another room.
                // don't create this room at all.
                goto next_room;
            }
        }
        rooms.append(room);
        next_room:;
    }
    List<Coord> room_floor_spaces;
    for (int i = 0; i < rooms.length(); i++) {
        Coord cursor;
        SDL_Rect room = rooms[i];
        for (cursor.y = room.y + 1; cursor.y < room.y + room.h - 1; cursor.y++) {
            for (cursor.x = room.x + 1; cursor.x < room.x + room.w - 1; cursor.x++) {
                room_floor_spaces.append(cursor);
                actual_map_tiles[cursor].tile_type = TileType_FLOOR;
            }
        }
    }

    // connect rooms with prim's algorithm.
    struct PrimUtil {
        static inline int find_root_node(const List<int> & node_to_parent_node, int node) {
            while (true) {
                int parent_node = node_to_parent_node[node];
                if (parent_node == node)
                    return node;
                node = parent_node;
            }
        }
        static inline void merge(List<int> * node_to_parent_node, int a, int b) {
            (*node_to_parent_node)[a] = b;
        }
    };
    List<int> node_to_parent_node;
    for (int i = 0; i < rooms.length(); i++)
        node_to_parent_node.append(i);
    for (int i = 0; i < rooms.length(); i++) {
        SDL_Rect room = rooms[i];
        int room_root = PrimUtil::find_root_node(node_to_parent_node, i);
        // find nearest room
        // TODO: this is not prim's algorithm. we're supposed to find the shortest edge in the graph.
        int closest_neighbor = -1;
        SDL_Rect closest_room = {-1, -1, -1, -1};
        int closest_neighbor_root = -1;
        int closest_distance = 0x7fffffff;
        for (int j = 0; j < rooms.length(); j++) {
            SDL_Rect other_room = rooms[j];
            int other_root = PrimUtil::find_root_node(node_to_parent_node, j);
            if (room_root == other_root)
                continue; // already joined
            int distance = ordinal_distance(Coord{room.x, room.y}, Coord{other_room.x, other_room.y});
            if (distance < closest_distance) {
                closest_neighbor = j;
                closest_room = other_room;
                closest_neighbor_root = other_root;
                closest_distance = distance;
            }
        }
        if (closest_neighbor == -1)
            continue; // already connected to everyone
        // connect to neighbor
        PrimUtil::merge(&node_to_parent_node, room_root, closest_neighbor_root);
        // derpizontal, and then derpicle
        Coord a = {room.x + 1, room.y + 1};
        Coord b = {closest_room.x + 1, closest_room.y + 1};
        Coord delta = sign(b - a);
        Coord cursor = a;
        for (; cursor.x * delta.x < b.x * delta.x; cursor.x += delta.x)
            actual_map_tiles[cursor].tile_type = TileType_FLOOR;
        for (; cursor.y * delta.y < b.y * delta.y; cursor.y += delta.y)
            actual_map_tiles[cursor].tile_type = TileType_FLOOR;
    }

    // place the stairs down
    stairs_down_location = room_floor_spaces[random_int(room_floor_spaces.length())];
    actual_map_tiles[stairs_down_location].tile_type = TileType_STAIRS_DOWN;

    // throw some items around
    int item_count = random_inclusive(1, 2);
    for (int i = 0; i < item_count; i++) {
        Coord location = room_floor_spaces[random_int(room_floor_spaces.length())];
        Thing item = random_item();
        item->location = location;
    }
}
