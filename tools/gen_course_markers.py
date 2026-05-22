#!/usr/bin/env python3
import argparse
import pathlib
import re
import xml.etree.ElementTree as ET


MARKER_BALL = "Ball"
MARKER_HOLE = "Hole"
LAYER_BALL_POS = "BallPos"
LAYER_HOLE_POS = "HolePos"
PROPERTY_PAR = "Par"
DEFINE_COURSE_DEFAULT_PAR = "COURSE_DEFAULT_PAR"
DEFINE_HUD_START_ROW = "HUD_START_ROW"
DEFINE_TILE_ID_WALL = "TILE_ID_WALL"
DEFINE_TILE_ID_WATER_FIRST = "TILE_ID_WATER_FIRST"
DEFINE_TILE_ID_WATER_LAST = "TILE_ID_WATER_LAST"
DEFINE_TILE_ID_RAMP_UP_LEFT = "TILE_ID_RAMP_UP_LEFT"
DEFINE_TILE_ID_RAMP_UP_RIGHT = "TILE_ID_RAMP_UP_RIGHT"
DEFINE_TILE_ID_RAMP_RIGHT_LEFT = "TILE_ID_RAMP_RIGHT_LEFT"
DEFINE_TILE_ID_RAMP_RIGHT_RIGHT = "TILE_ID_RAMP_RIGHT_RIGHT"


def parse_args():
    parser = argparse.ArgumentParser(description="Generate minigolf course metadata from Tiled point markers.")
    parser.add_argument("--tmx", required=True, help="Source TMX path.")
    parser.add_argument("--course-width-tiles", required=True, type=int, help="Course width in tiles.")
    parser.add_argument("--course-height-tiles", required=True, type=int, help="Course height in tiles.")
    parser.add_argument("--tile-size", required=True, type=int, help="Tile size in pixels.")
    parser.add_argument("--course-count", required=True, type=int, help="Expected gameplay course count.")
    parser.add_argument("--config", required=True, help="Config header containing COURSE_DEFAULT_PAR.")
    parser.add_argument("--output", required=True, help="Generated include path.")
    return parser.parse_args()


def course_grid(root, course_w, course_h, course_count):
    map_w = int(root.attrib["width"])
    map_h = int(root.attrib["height"])

    if map_w % course_w != 0 or map_h % course_h != 0:
        raise ValueError(
            f"TMX map {map_w}x{map_h} is not divisible by course size {course_w}x{course_h}."
        )

    course_cols = map_w // course_w
    course_rows = map_h // course_h
    grid_count = course_cols * course_rows

    if grid_count != course_count:
        raise ValueError(
            f"TMX course grid contains {grid_count} courses, but GAME_COURSE_COUNT is {course_count}."
        )

    return course_cols, course_rows


def numeric_define_from_config(config_path, name):
    pattern = re.compile(rf"^\s*#define\s+{name}\s+([0-9]+)\s*$")

    for line in config_path.read_text(encoding="ascii").splitlines():
        match = pattern.match(line)
        if match is not None:
            return int(match.group(1))

    raise ValueError(f"{name} was not found in {config_path}.")


def firstgid(root):
    tileset = root.find("./tileset")
    if tileset is None:
        raise ValueError("TMX file does not contain a tileset.")

    return int(tileset.attrib.get("firstgid", "1"))


def read_tile_layer(root):
    layer = root.find("./layer")
    if layer is None:
        raise ValueError("TMX file does not contain a tile layer.")

    data = layer.find("./data")
    if data is None or data.attrib.get("encoding") != "csv":
        raise ValueError("TMX tile layer must use CSV encoding.")

    map_w = int(root.attrib["width"])
    map_h = int(root.attrib["height"])
    values = [
        int(value.strip())
        for value in data.text.replace("\n", "").split(",")
        if value.strip()
    ]

    expected_values = map_w * map_h
    if len(values) != expected_values:
        raise ValueError(
            f"TMX tile layer contains {len(values)} CSV values, expected {expected_values}."
        )

    return map_w, map_h, values


def empty_courses(course_count, default_par):
    return [
        {
            "ball": None,
            "hole": None,
            "par_expr": "COURSE_DEFAULT_PAR",
            "par_comment": default_par,
        }
        for _ in range(course_count)
    ]


def marker_kind(name):
    if name == MARKER_BALL:
        return "ball"

    if name == MARKER_HOLE:
        return "hole"

    return None


def validate_marker_layer(kind, layer_name, obj):
    expected_layer = LAYER_BALL_POS if kind == "ball" else LAYER_HOLE_POS

    if layer_name == expected_layer:
        return

    raise ValueError(
        f"{obj.attrib.get('name', '')} marker id {obj.attrib.get('id', '?')} "
        f"must be in object layer {expected_layer}, but is in {layer_name}."
    )


def object_property(obj, name):
    for prop in obj.findall("./properties/property"):
        if prop.attrib.get("name") == name:
            return prop.attrib.get("value")

    return None


def object_par(obj):
    value = object_property(obj, PROPERTY_PAR)

    if value is None:
        return None

    par = int(value)
    if par < 1 or par > 9:
        raise ValueError(f"Par value must be between 1 and 9, got {par}.")

    return str(par)


def marker_tile_id(tile_values, map_w, tile_x, tile_y, gid_offset):
    gid = tile_values[(tile_y * map_w) + tile_x]

    if gid == 0:
        return None

    return gid - gid_offset


def local_course_tile(tile_x, tile_y, course_w, course_h, course_cols):
    course_col = tile_x // course_w
    course_row = tile_y // course_h
    course_index = (course_row * course_cols) + course_col

    return course_index, tile_x % course_w, tile_y % course_h


def tile_at_or_none(tile_values, map_w, map_h, tile_x, tile_y, gid_offset):
    if tile_x < 0 or tile_y < 0 or tile_x >= map_w or tile_y >= map_h:
        return None

    return marker_tile_id(tile_values, map_w, tile_x, tile_y, gid_offset)


def raise_ramp_pair_error(tile_x, tile_y, tile_id, course_w, course_h, course_cols, message):
    course_index, local_x, local_y = local_course_tile(tile_x, tile_y, course_w, course_h, course_cols)

    raise ValueError(
        f"Course {course_index} ramp tile ID {tile_id} at local tile {local_x},{local_y} {message}."
    )


def validate_ramp_pairs(
    tile_values,
    map_w,
    map_h,
    gid_offset,
    course_w,
    course_h,
    course_cols,
    ramp_up_left,
    ramp_up_right,
    ramp_right_left,
    ramp_right_right):
    for index, gid in enumerate(tile_values):
        if gid == 0:
            continue

        tile_id = gid - gid_offset
        tile_x = index % map_w
        tile_y = index // map_w

        if tile_id == ramp_up_left:
            if tile_at_or_none(tile_values, map_w, map_h, tile_x + 1, tile_y, gid_offset) != ramp_up_right:
                raise_ramp_pair_error(tile_x, tile_y, tile_id, course_w, course_h, course_cols,
                    f"must be immediately followed by tile ID {ramp_up_right} to the right")
        elif tile_id == ramp_up_right:
            if tile_at_or_none(tile_values, map_w, map_h, tile_x - 1, tile_y, gid_offset) != ramp_up_left:
                raise_ramp_pair_error(tile_x, tile_y, tile_id, course_w, course_h, course_cols,
                    f"must be immediately preceded by tile ID {ramp_up_left} to the left")
        elif tile_id == ramp_right_left:
            if tile_at_or_none(tile_values, map_w, map_h, tile_x, tile_y + 1, gid_offset) != ramp_right_right:
                raise_ramp_pair_error(tile_x, tile_y, tile_id, course_w, course_h, course_cols,
                    f"must be immediately followed by tile ID {ramp_right_right} below")
        elif tile_id == ramp_right_right:
            if tile_at_or_none(tile_values, map_w, map_h, tile_x, tile_y - 1, gid_offset) != ramp_right_left:
                raise_ramp_pair_error(tile_x, tile_y, tile_id, course_w, course_h, course_cols,
                    f"must be immediately preceded by tile ID {ramp_right_left} above")


def marker_tile_is_forbidden(tile_id, wall_tile, water_first, water_last):
    if tile_id is None:
        return True

    return tile_id == wall_tile or water_first <= tile_id <= water_last


def validate_marker_tile(kind, course_index, local_x, local_y, tile_id, wall_tile, water_first, water_last):
    if not marker_tile_is_forbidden(tile_id, wall_tile, water_first, water_last):
        return

    label = "empty tile" if tile_id is None else f"tile ID {tile_id}"
    raise ValueError(
        f"Course {course_index} {kind} marker is at local tile {local_x},{local_y} "
        f"on {label}. Ball/Hole markers must be on playable floor."
    )


def add_marker(
    courses,
    kind,
    obj,
    course_cols,
    course_rows,
    course_w,
    course_h,
    playfield_h,
    tile_size,
    tile_values,
    map_w,
    gid_offset,
    wall_tile,
    water_first,
    water_last):
    pixel_x = float(obj.attrib["x"])
    pixel_y = float(obj.attrib["y"])
    tile_x = int(pixel_x // tile_size)
    tile_y = int(pixel_y // tile_size)
    course_col = tile_x // course_w
    course_row = tile_y // course_h

    if course_col >= course_cols or course_row >= course_rows:
        raise ValueError(
            f"Marker {obj.attrib.get('name', '')} at tile {tile_x},{tile_y} is outside the course grid."
        )

    course_index = (course_row * course_cols) + course_col
    if course_index >= len(courses):
        raise ValueError(
            f"Marker {obj.attrib.get('name', '')} maps to course {course_index}, "
            f"but only {len(courses)} courses are configured."
        )

    local_x = tile_x % course_w
    local_y = tile_y % course_h
    tile_id = marker_tile_id(tile_values, map_w, tile_x, tile_y, gid_offset)

    if local_y >= playfield_h:
        raise ValueError(
            f"Course {course_index} {kind} marker is at local tile {local_x},{local_y}, "
            f"inside the HUD/non-playable rows. Playfield height is {playfield_h} tiles."
        )

    validate_marker_tile(kind, course_index, local_x, local_y, tile_id, wall_tile, water_first, water_last)

    if courses[course_index][kind] is not None:
        raise ValueError(f"Course {course_index} has more than one {kind} marker.")

    courses[course_index][kind] = (local_x, local_y)

    if kind == "hole":
        par = object_par(obj)
        if par is not None:
            courses[course_index]["par_expr"] = par
            courses[course_index]["par_comment"] = par


def read_markers(
    tmx_path,
    course_w,
    course_h,
    playfield_h,
    tile_size,
    course_count,
    default_par,
    wall_tile,
    water_first,
    water_last,
    ramp_up_left,
    ramp_up_right,
    ramp_right_left,
    ramp_right_right):
    root = ET.parse(tmx_path).getroot()
    course_cols, course_rows = course_grid(root, course_w, course_h, course_count)
    map_w, map_h, tile_values = read_tile_layer(root)
    gid_offset = firstgid(root)
    courses = empty_courses(course_count, default_par)

    validate_ramp_pairs(
        tile_values,
        map_w,
        map_h,
        gid_offset,
        course_w,
        course_h,
        course_cols,
        ramp_up_left,
        ramp_up_right,
        ramp_right_left,
        ramp_right_right)

    for objectgroup in root.findall("./objectgroup"):
        layer_name = objectgroup.attrib.get("name", "")
        for obj in objectgroup.findall("./object"):
            kind = marker_kind(obj.attrib.get("name", ""))
            if kind is None:
                continue

            validate_marker_layer(kind, layer_name, obj)
            add_marker(
                courses,
                kind,
                obj,
                course_cols,
                course_rows,
                course_w,
                course_h,
                playfield_h,
                tile_size,
                tile_values,
                map_w,
                gid_offset,
                wall_tile,
                water_first,
                water_last)

    for index, course in enumerate(courses):
        if course["ball"] is None:
            raise ValueError(f"Course {index} is missing a Ball marker.")

        if course["hole"] is None:
            raise ValueError(f"Course {index} is missing a Hole marker.")

        if course["ball"] == course["hole"]:
            raise ValueError(f"Course {index} Ball and Hole markers share tile {course['ball']}.")

    return courses


def write_output(path, courses):
    lines = [
        "/* Auto-generated by tools/gen_course_markers.py. Do not edit manually. */",
        "#include \"config.h\"",
        "#include \"course.h\"",
        "",
        "const course_info_t course_infos[GAME_COURSE_COUNT] = {",
    ]

    for index, course in enumerate(courses):
        ball_x, ball_y = course["ball"]
        hole_x, hole_y = course["hole"]
        lines.append(
            f"    {{ {ball_x}, {ball_y}, {hole_x}, {hole_y}, {course['par_expr']} }}, "
            f"/* course {index}: ball=({ball_x},{ball_y}), hole=({hole_x},{hole_y}), par={course['par_comment']} */"
        )

    lines.append("};")

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="ascii")


def main():
    args = parse_args()
    config_path = pathlib.Path(args.config)
    default_par = str(numeric_define_from_config(config_path, DEFINE_COURSE_DEFAULT_PAR))
    playfield_h = numeric_define_from_config(config_path, DEFINE_HUD_START_ROW)
    wall_tile = numeric_define_from_config(config_path, DEFINE_TILE_ID_WALL)
    water_first = numeric_define_from_config(config_path, DEFINE_TILE_ID_WATER_FIRST)
    water_last = numeric_define_from_config(config_path, DEFINE_TILE_ID_WATER_LAST)
    ramp_up_left = numeric_define_from_config(config_path, DEFINE_TILE_ID_RAMP_UP_LEFT)
    ramp_up_right = numeric_define_from_config(config_path, DEFINE_TILE_ID_RAMP_UP_RIGHT)
    ramp_right_left = numeric_define_from_config(config_path, DEFINE_TILE_ID_RAMP_RIGHT_LEFT)
    ramp_right_right = numeric_define_from_config(config_path, DEFINE_TILE_ID_RAMP_RIGHT_RIGHT)
    courses = read_markers(
        pathlib.Path(args.tmx),
        args.course_width_tiles,
        args.course_height_tiles,
        playfield_h,
        args.tile_size,
        args.course_count,
        default_par,
        wall_tile,
        water_first,
        water_last,
        ramp_up_left,
        ramp_up_right,
        ramp_right_left,
        ramp_right_right,
    )
    write_output(pathlib.Path(args.output), courses)


if __name__ == "__main__":
    main()
