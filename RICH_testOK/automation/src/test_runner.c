#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "game_engine.h"

/* ------------------------------------------------------------------------- */
/* 小工具函数                                                                  */
/* ------------------------------------------------------------------------- */

static char *read_text_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        return NULL;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }

    long size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return NULL;
    }
    rewind(fp);

    char *buffer = (char *)malloc((size_t)size + 1);
    if (buffer == NULL) {
        fclose(fp);
        return NULL;
    }

    size_t read_size = fread(buffer, 1, (size_t)size, fp);
    fclose(fp);

    if (read_size != (size_t)size) {
        free(buffer);
        return NULL;
    }

    buffer[size] = '\0';
    return buffer;
}

static int scalar_is_equal(const cJSON *a, const cJSON *b) {
    if (a == NULL || b == NULL) {
        return 0;
    }

    if (cJSON_IsNull(a) && cJSON_IsNull(b)) {
        return 1;
    }
    if (cJSON_IsBool(a) && cJSON_IsBool(b)) {
        return cJSON_IsTrue(a) == cJSON_IsTrue(b);
    }
    if (cJSON_IsNumber(a) && cJSON_IsNumber(b)) {
        long long va = (long long)cJSON_GetNumberValue(a);
        long long vb = (long long)cJSON_GetNumberValue(b);
        return va == vb;
    }
    if (cJSON_IsString(a) && cJSON_IsString(b)) {
        const char *sa = cJSON_GetStringValue(a);
        const char *sb = cJSON_GetStringValue(b);
        if (sa == NULL || sb == NULL) {
            return 0;
        }
        return strcmp(sa, sb) == 0;
    }

    return 0;
}

static const char *scalar_value_string(const cJSON *item) {
    if (item == NULL) {
        return "<missing>";
    }
    if (cJSON_IsNull(item)) {
        return "null";
    }
    if (cJSON_IsBool(item)) {
        return cJSON_IsTrue(item) ? "true" : "false";
    }
    if (cJSON_IsNumber(item)) {
        static char buf[32];
        snprintf(buf, sizeof(buf), "%lld", (long long)cJSON_GetNumberValue(item));
        return buf;
    }
    if (cJSON_IsString(item)) {
        const char *s = cJSON_GetStringValue(item);
        return s == NULL ? "" : s;
    }
    return "<object/array>";
}

static cJSON *add_error(
    cJSON *errors,
    const char *code,
    const char *path,
    const cJSON *expected,
    const cJSON *actual,
    const char *message)
{
    cJSON *error = cJSON_CreateObject();
    if (error == NULL) {
        return NULL;
    }

    cJSON_AddStringToObject(error, "code", code);
    cJSON_AddStringToObject(error, "path", path == NULL ? "" : path);
    cJSON_AddStringToObject(error, "message", message == NULL ? "" : message);

    if (expected != NULL) {
        if (cJSON_IsNumber(expected)) {
            cJSON_AddNumberToObject(error, "expected", cJSON_GetNumberValue(expected));
        } else if (cJSON_IsString(expected)) {
            cJSON_AddStringToObject(error, "expected", cJSON_GetStringValue(expected));
        } else if (cJSON_IsBool(expected)) {
            cJSON_AddBoolToObject(error, "expected", cJSON_IsTrue(expected) ? 1 : 0);
        } else {
            cJSON_AddStringToObject(error, "expected", scalar_value_string(expected));
        }
    }

    if (actual != NULL) {
        if (cJSON_IsNumber(actual)) {
            cJSON_AddNumberToObject(error, "actual", cJSON_GetNumberValue(actual));
        } else if (cJSON_IsString(actual)) {
            cJSON_AddStringToObject(error, "actual", cJSON_GetStringValue(actual));
        } else if (cJSON_IsBool(actual)) {
            cJSON_AddBoolToObject(error, "actual", cJSON_IsTrue(actual) ? 1 : 0);
        } else {
            cJSON_AddStringToObject(error, "actual", scalar_value_string(actual));
        }
    }

    cJSON_AddItemToArray(errors, error);
    return error;
}

/* ------------------------------------------------------------------------- */
/* Expected 部分匹配                                                           */
/* ------------------------------------------------------------------------- */

static const char *array_primary_key(const char *array_name) {
    if (array_name == NULL) {
        return NULL;
    }
    if (strcmp(array_name, "players") == 0) {
        return "id";
    }
    if (strcmp(array_name, "properties") == 0 ||
        strcmp(array_name, "map_items") == 0 ||
        strcmp(array_name, "display_players") == 0) {
        return "position";
    }
    return NULL;
}

static cJSON *find_array_item_by_key(
    const cJSON *array,
    const char *key_name,
    const cJSON *expected_item)
{
    if (!cJSON_IsArray(array) || key_name == NULL || expected_item == NULL) {
        return NULL;
    }

    cJSON *expected_key = cJSON_GetObjectItemCaseSensitive(expected_item, key_name);
    if (expected_key == NULL) {
        return NULL;
    }

    int count = cJSON_GetArraySize(array);
    for (int i = 0; i < count; i++) {
        cJSON *actual_item = cJSON_GetArrayItem(array, i);
        cJSON *actual_key = cJSON_GetObjectItemCaseSensitive(actual_item, key_name);
        if (scalar_is_equal(expected_key, actual_key)) {
            return actual_item;
        }
    }

    return NULL;
}

static void compare_node(
    const cJSON *expected,
    const cJSON *actual,
    const char *path,
    const char *array_name,
    cJSON *errors);

static void compare_array(
    const cJSON *expected_array,
    const cJSON *actual_array,
    const char *path,
    const char *array_name,
    cJSON *errors)
{
    const char *key_name = array_primary_key(array_name);

    int expected_count = cJSON_GetArraySize(expected_array);
    for (int i = 0; i < expected_count; i++) {
        cJSON *expected_item = cJSON_GetArrayItem(expected_array, i);
        char item_path[1024];

        if (key_name != NULL) {
            cJSON *key_item = cJSON_GetObjectItemCaseSensitive(expected_item, key_name);
            cJSON *actual_item = find_array_item_by_key(actual_array, key_name, expected_item);

            snprintf(
                item_path,
                sizeof(item_path),
                "%s[%s=%s]",
                path == NULL ? "" : path,
                key_name,
                key_item == NULL ? "?" : scalar_value_string(key_item));

            if (actual_item == NULL) {
                add_error(
                    errors,
                    "ASSERT_NOT_FOUND",
                    item_path,
                    NULL,
                    NULL,
                    "Expected item was not found in Actual");
                continue;
            }

            compare_node(expected_item, actual_item, item_path, NULL, errors);
        } else {
            snprintf(item_path, sizeof(item_path), "%s[%d]", path == NULL ? "" : path, i);

            cJSON *actual_item = cJSON_GetArrayItem(actual_array, i);
            if (actual_item == NULL) {
                add_error(
                    errors,
                    "ASSERT_NOT_FOUND",
                    item_path,
                    NULL,
                    NULL,
                    "Expected index was not found in Actual");
                continue;
            }

            compare_node(expected_item, actual_item, item_path, NULL, errors);
        }
    }
}

static void compare_absent_positions(
    const char *array_name,
    const cJSON *actual_root,
    const cJSON *positions,
    const char *path,
    cJSON *errors)
{
    if (!cJSON_IsArray(positions)) {
        return;
    }

    const cJSON *actual_array = cJSON_GetObjectItemCaseSensitive(actual_root, array_name);
    int count = cJSON_GetArraySize(positions);

    for (int i = 0; i < count; i++) {
        cJSON *position_item = cJSON_GetArrayItem(positions, i);
        if (!cJSON_IsNumber(position_item)) {
            continue;
        }

        int expected_absent = (int)cJSON_GetNumberValue(position_item);
        int found = 0;

        if (cJSON_IsArray(actual_array)) {
            int actual_count = cJSON_GetArraySize(actual_array);
            for (int j = 0; j < actual_count; j++) {
                cJSON *item = cJSON_GetArrayItem(actual_array, j);
                cJSON *position = cJSON_GetObjectItemCaseSensitive(item, "position");
                if (cJSON_IsNumber(position) &&
                    (int)cJSON_GetNumberValue(position) == expected_absent) {
                    found = 1;
                    break;
                }
            }
        }

        if (found) {
            char item_path[1024];
            snprintf(
                item_path,
                sizeof(item_path),
                "%s.%s[position=%d]",
                path == NULL ? "" : path,
                array_name,
                expected_absent);
            add_error(
                errors,
                "ASSERT_NOT_ABSENT",
                item_path,
                NULL,
                NULL,
                "This position should be absent, but it exists in Actual");
        }
    }
}

static void compare_object(
    const cJSON *expected,
    const cJSON *actual,
    const char *path,
    cJSON *errors)
{
    const cJSON *child = expected->child;
    while (child != NULL) {
        const char *key = child->string;

        if (strcmp(key, "properties_absent") == 0) {
            compare_absent_positions("properties", actual, child, path, errors);
            child = child->next;
            continue;
        }
        if (strcmp(key, "map_items_absent") == 0) {
            compare_absent_positions("map_items", actual, child, path, errors);
            child = child->next;
            continue;
        }

        cJSON *actual_child = cJSON_GetObjectItemCaseSensitive(actual, key);
        char child_path[1024];
        snprintf(
            child_path,
            sizeof(child_path),
            "%s.%s",
            path == NULL || path[0] == '\0' ? "actual" : path,
            key);

        if (actual_child == NULL) {
            add_error(
                errors,
                "ASSERT_NOT_FOUND",
                child_path,
                NULL,
                NULL,
                "Expected field was not found in Actual");
            child = child->next;
            continue;
        }

        compare_node(child, actual_child, child_path, key, errors);
        child = child->next;
    }
}

static void compare_node(
    const cJSON *expected,
    const cJSON *actual,
    const char *path,
    const char *array_name,
    cJSON *errors)
{
    if (expected == NULL || actual == NULL) {
        return;
    }

    if (cJSON_IsObject(expected)) {
        if (!cJSON_IsObject(actual)) {
            add_error(
                errors,
                "ASSERT_NOT_EQUAL",
                path,
                expected,
                actual,
                "Type mismatch: expected object, actual is not object");
            return;
        }
        compare_object(expected, actual, path, errors);
        return;
    }

    if (cJSON_IsArray(expected)) {
        if (!cJSON_IsArray(actual)) {
            add_error(
                errors,
                "ASSERT_NOT_EQUAL",
                path,
                expected,
                actual,
                "Type mismatch: expected array, actual is not array");
            return;
        }
        compare_array(expected, actual, path, array_name, errors);
        return;
    }

    if (!scalar_is_equal(expected, actual)) {
        add_error(
            errors,
            "ASSERT_NOT_EQUAL",
            path,
            expected,
            actual,
            "Scalar values are not equal");
    }
}

/* ------------------------------------------------------------------------- */
/* 主流程                                                                      */
/* ------------------------------------------------------------------------- */

static int run_one_case(const char *file_path) {
    char *text = read_text_file(file_path);
    if (text == NULL) {
        printf("{\"case_id\":\"%s\",\"result\":\"ERROR\",\"errors\":[{\"code\":\"FILE_READ\",\"message\":\"Unable to read file\"}]}\n", file_path);
        return 1;
    }

    cJSON *root = cJSON_Parse(text);
    free(text);

    if (root == NULL) {
        printf("{\"case_id\":\"%s\",\"result\":\"ERROR\",\"errors\":[{\"code\":\"INVALID_JSON\",\"message\":\"Unable to parse JSON\"}]}\n", file_path);
        return 1;
    }

    cJSON *schema = cJSON_GetObjectItemCaseSensitive(root, "schema_version");
    cJSON *case_id = cJSON_GetObjectItemCaseSensitive(root, "case_id");
    cJSON *preset = cJSON_GetObjectItemCaseSensitive(root, "preset");
    cJSON *actions = cJSON_GetObjectItemCaseSensitive(root, "actions");
    cJSON *expected = cJSON_GetObjectItemCaseSensitive(root, "expected");

    const char *case_id_str = (case_id != NULL && cJSON_IsString(case_id))
                                  ? cJSON_GetStringValue(case_id)
                                  : file_path;

    cJSON *result = cJSON_CreateObject();
    cJSON *actual = NULL;
    cJSON_AddStringToObject(result, "schema_version", "1.0");
    cJSON_AddStringToObject(result, "case_id", case_id_str);

    if (schema == NULL || !cJSON_IsString(schema) ||
        strcmp(cJSON_GetStringValue(schema), "1.0") != 0) {
        cJSON_AddStringToObject(result, "result", "ERROR");
        cJSON *errors = cJSON_CreateArray();
        add_error(errors, "UNSUPPORTED_VERSION", "schema_version", NULL, NULL, "Unsupported schema_version");
        cJSON_AddItemToObject(result, "errors", errors);
        goto done;
    }

    if (!cJSON_IsObject(preset) || !cJSON_IsArray(actions) || !cJSON_IsObject(expected)) {
        cJSON_AddStringToObject(result, "result", "ERROR");
        cJSON *errors = cJSON_CreateArray();
        add_error(errors, "INVALID_PRESET", "root", NULL, NULL, "preset/actions/expected is missing or has wrong type");
        cJSON_AddItemToObject(result, "errors", errors);
        goto done;
    }

    char *engine_code = NULL;
    char *engine_message = NULL;
    actual = game_engine_execute(preset, actions, &engine_code, &engine_message);

    if (actual == NULL) {
        cJSON_AddStringToObject(result, "result", "ERROR");
        cJSON *errors = cJSON_CreateArray();
        cJSON *error = cJSON_CreateObject();
        cJSON_AddStringToObject(error, "code", engine_code == NULL ? "ENGINE_ERROR" : engine_code);
        cJSON_AddStringToObject(error, "message", engine_message == NULL ? "Game engine execution failed" : engine_message);
        cJSON_AddItemToArray(errors, error);
        cJSON_AddItemToObject(result, "errors", errors);
        goto done;
    }

    {
        cJSON *errors = cJSON_CreateArray();
        compare_node(expected, actual, "actual", NULL, errors);

        if (cJSON_GetArraySize(errors) == 0) {
            cJSON_AddStringToObject(result, "result", "PASS");
        } else {
            cJSON_AddStringToObject(result, "result", "FAIL");
            cJSON_AddItemToObject(result, "errors", errors);
        }

        cJSON_AddItemToObject(result, "actual", actual);
        actual = NULL;
    }

done:
    if (actual != NULL) {
        cJSON_Delete(actual);
    }

    char *printed = cJSON_Print(result);
    if (printed != NULL) {
        printf("%s\n", printed);
        free(printed);
    }

    cJSON_Delete(result);
    cJSON_Delete(root);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: rich_test.exe <testcase.json> [testcase2.json ...]\n");
        return 2;
    }

    int failed = 0;
    for (int i = 1; i < argc; i++) {
        if (run_one_case(argv[i]) != 0) {
            failed = 1;
        }
    }

    return failed;
}
