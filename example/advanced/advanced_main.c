/**
 * Advanced json-gen-c example
 *
 * Demonstrates all major features:
 *   1. Enums and default values
 *   2. Optional and nullable fields
 *   3. Field aliases (@json)
 *   4. Maps (key-value dictionaries)
 *   5. Dynamic and fixed-size arrays
 *   6. Nested structs
 *   7. Tagged unions (oneof)
 *   8. Selective parsing
 *   9. Pretty-printing with indentation
 *
 * Build and run:
 *   make run
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json.gen.h"
#include "sstr.h"

/* ---------- helpers ---------- */

static void print_separator(const char* title) {
    printf("\n======== %s ========\n\n", title);
}

static void check(int rc, const char* msg) {
    if (rc != 0) {
        fprintf(stderr, "ERROR: %s (rc=%d)\n", msg, rc);
        exit(1);
    }
}

/* ---------- 1. Enums and defaults ---------- */

static void example_enums_and_defaults(void) {
    print_separator("Enums & Default Values");

    struct Task task;
    Task_init(&task);

    /* After init, defaults are applied */
    printf("After init (defaults applied):\n");
    printf("  priority        = %d (Priority_MEDIUM=%d)\n",
           task.priority, Priority_MEDIUM);
    printf("  status          = %d (Status_PENDING=%d)\n",
           task.status, Status_PENDING);
    printf("  estimated_hours = %d\n", task.estimated_hours);

    /* Set required fields */
    task.id = 1;
    task.title = sstr("Implement feature X");

    sstr_t json = sstr_new();
    check(json_marshal_indent_Task(&task, 2, 0, json), "marshal task");
    printf("\nMarshal with defaults:\n%s\n", sstr_cstr(json));

    sstr_free(json);
    Task_clear(&task);
}

/* ---------- 2. Optional and nullable ---------- */

static void example_optional_nullable(void) {
    print_separator("Optional & Nullable Fields");

    /* Task has: optional description, nullable assignee */
    struct Task task;
    Task_init(&task);
    task.id = 2;
    task.title = sstr("Review PR #42");
    task.priority = Priority_HIGH;

    /* Optional field: omit description → not in JSON */
    /* has_description defaults to false from init */

    /* Nullable field: assignee stays NULL from init */

    sstr_t json1 = sstr_new();
    check(json_marshal_indent_Task(&task, 2, 0, json1), "marshal");
    printf("No optional/nullable set:\n%s\n", sstr_cstr(json1));

    /* Now set the optional and nullable fields */
    task.has_description = true;
    task.description = sstr("Please review the changes in module Y");
    task.has_assignee = true;
    task.assignee = sstr("alice");

    sstr_t json2 = sstr_new();
    check(json_marshal_indent_Task(&task, 2, 0, json2), "marshal");
    printf("\nWith optional & nullable set:\n%s\n", sstr_cstr(json2));

    sstr_free(json1);
    sstr_free(json2);
    Task_clear(&task);
}

/* ---------- 3. Field aliases ---------- */

static void example_aliases(void) {
    print_separator("Field Aliases (@json)");

    /* Task.id → "task_id", Task.priority → "pri", Task.tags → "labels" */
    const char* input =
        "{\"task_id\":99,\"title\":\"Aliased task\","
        "\"pri\":\"CRITICAL\",\"status\":\"ACTIVE\","
        "\"labels\":[\"urgent\",\"backend\"],"
        "\"estimated_hours\":16}";

    printf("Input JSON (with aliases):\n  %s\n\n", input);

    sstr_t json = sstr_of(input, strlen(input));
    struct Task task;
    Task_init(&task);
    check(json_unmarshal_Task(json, &task), "unmarshal aliased");

    printf("Parsed values:\n");
    printf("  id (from \"task_id\")       = %d\n", task.id);
    printf("  title                      = %s\n", sstr_cstr(task.title));
    printf("  priority (from \"pri\")      = %d (CRITICAL=%d)\n",
           task.priority, Priority_CRITICAL);
    printf("  status                     = %d (ACTIVE=%d)\n",
           task.status, Status_ACTIVE);
    printf("  tags[0] (from \"labels\")    = %s\n", sstr_cstr(task.tags[0]));
    printf("  tags[1]                    = %s\n", sstr_cstr(task.tags[1]));
    printf("  estimated_hours            = %d\n", task.estimated_hours);

    sstr_free(json);
    Task_clear(&task);
}

/* ---------- 4. Maps ---------- */

static void example_maps(void) {
    print_separator("Maps (Key-Value Dictionaries)");

    struct Dashboard dash;
    Dashboard_init(&dash);
    dash.name = sstr("Sprint Board");

    /* Populate task_counts map: {key, value} entries */
    struct json_map_entry_int tc_entries[3];
    tc_entries[0].key = sstr("todo");
    tc_entries[0].value = 12;
    tc_entries[1].key = sstr("in_progress");
    tc_entries[1].value = 5;
    tc_entries[2].key = sstr("done");
    tc_entries[2].value = 23;
    dash.task_counts.entries = tc_entries;
    dash.task_counts.len = 3;

    /* Populate settings map */
    struct json_map_entry_sstr_t st_entries[2];
    st_entries[0].key = sstr("theme");
    st_entries[0].value = sstr("dark");
    st_entries[1].key = sstr("language");
    st_entries[1].value = sstr("en");
    dash.settings.entries = st_entries;
    dash.settings.len = 2;

    sstr_t json = sstr_new();
    check(json_marshal_indent_Dashboard(&dash, 2, 0, json), "marshal maps");
    printf("Dashboard with maps:\n%s\n", sstr_cstr(json));

    /* Round-trip: unmarshal → re-marshal */
    struct Dashboard dash2;
    Dashboard_init(&dash2);
    check(json_unmarshal_Dashboard(json, &dash2), "unmarshal maps");

    sstr_t json2 = sstr_new();
    check(json_marshal_indent_Dashboard(&dash2, 2, 0, json2),
          "re-marshal maps");
    printf("\nRound-trip result:\n%s\n", sstr_cstr(json2));

    sstr_free(json);
    sstr_free(json2);
    /* Don't clear dash — entries are stack-allocated; free keys manually */
    sstr_free(tc_entries[0].key);
    sstr_free(tc_entries[1].key);
    sstr_free(tc_entries[2].key);
    sstr_free(st_entries[0].key);
    sstr_free(st_entries[0].value);
    sstr_free(st_entries[1].key);
    sstr_free(st_entries[1].value);
    sstr_free(dash.name);
    Dashboard_clear(&dash2);
}

/* ---------- 5. Oneof (tagged union) ---------- */

static void example_oneof(void) {
    print_separator("Tagged Unions (oneof)");

    /* Create an email notification */
    struct Notification notif;
    Notification_init(&notif);
    notif.tag = Notification_email;
    EmailNotification_init(&notif.value.email);
    notif.value.email.to = sstr("team@example.com");
    notif.value.email.subject = sstr("Build succeeded");

    sstr_t json = sstr_new();
    check(json_marshal_indent_Notification(&notif, 2, 0, json),
          "marshal oneof");
    printf("Email notification:\n%s\n", sstr_cstr(json));
    Notification_clear(&notif);
    sstr_free(json);

    /* Parse a slack notification from JSON */
    const char* slack_json =
        "{\"method\":\"slack\",\"channel\":\"#deploys\","
        "\"message\":\"v2.1 deployed to prod\"}";
    printf("\nParsing: %s\n", slack_json);

    sstr_t input = sstr_of(slack_json, strlen(slack_json));
    Notification_init(&notif);
    check(json_unmarshal_Notification(input, &notif), "unmarshal oneof");

    printf("Parsed oneof tag: %d (slack=%d)\n",
           notif.tag, Notification_slack);
    printf("Channel: %s\n", sstr_cstr(notif.value.slack.channel));
    printf("Message: %s\n", sstr_cstr(notif.value.slack.message));

    sstr_free(input);
    Notification_clear(&notif);

    /* Webhook with default timeout */
    const char* webhook_json =
        "{\"method\":\"webhook\",\"url\":\"https://hooks.example.com/build\","
        "\"payload\":\"{}\"}";
    input = sstr_of(webhook_json, strlen(webhook_json));
    Notification_init(&notif);
    check(json_unmarshal_Notification(input, &notif), "unmarshal webhook");
    printf("\nWebhook timeout (default): %d ms\n",
           notif.value.webhook.timeout_ms);

    sstr_free(input);
    Notification_clear(&notif);
}

/* ---------- 6. Nested struct + arrays ---------- */

static void example_full_project(void) {
    print_separator("Full Project (nested struct, arrays, oneof)");

    struct Project proj;
    Project_init(&proj);

    proj.name = sstr("json-gen-c");
    proj.version = 1;
    proj.has_description = true;
    proj.description = sstr("Code generator for JSON serialization in C");

    /* Headquarters (nested struct with optional field) */
    proj.headquarters.street = sstr("123 Code St");
    proj.headquarters.city = sstr("San Francisco");
    proj.headquarters.country = sstr("US");
    proj.headquarters.has_zip_code = true;
    proj.headquarters.zip_code = sstr("94105");

    /* Tasks (dynamic array) */
    proj.tasks_len = 2;
    proj.tasks = (struct Task*)calloc(2, sizeof(struct Task));

    Task_init(&proj.tasks[0]);
    proj.tasks[0].id = 1;
    proj.tasks[0].title = sstr("Parser optimization");
    proj.tasks[0].priority = Priority_HIGH;
    proj.tasks[0].status = Status_ACTIVE;
    proj.tasks[0].has_assignee = true;
    proj.tasks[0].assignee = sstr("alice");
    proj.tasks[0].tags_len = 2;
    proj.tasks[0].tags = (sstr_t*)calloc(2, sizeof(sstr_t));
    proj.tasks[0].tags[0] = sstr("performance");
    proj.tasks[0].tags[1] = sstr("core");

    Task_init(&proj.tasks[1]);
    proj.tasks[1].id = 2;
    proj.tasks[1].title = sstr("Write documentation");
    proj.tasks[1].has_description = true;
    proj.tasks[1].description = sstr("Update README and examples");
    /* priority and status use defaults: MEDIUM, PENDING */

    /* Metadata (fixed-size array of 3) */
    Tag_init(&proj.metadata[0]);
    proj.metadata[0].key = sstr("lang");
    proj.metadata[0].value = sstr("C");
    Tag_init(&proj.metadata[1]);
    proj.metadata[1].key = sstr("license");
    proj.metadata[1].value = sstr("MIT");
    Tag_init(&proj.metadata[2]);
    proj.metadata[2].key = sstr("ci");
    proj.metadata[2].value = sstr("github-actions");

    /* Notifications (array of oneof) */
    proj.notifications_len = 2;
    proj.notifications =
        (struct Notification*)calloc(2, sizeof(struct Notification));
    Notification_init(&proj.notifications[0]);
    proj.notifications[0].tag = Notification_email;
    EmailNotification_init(&proj.notifications[0].value.email);
    proj.notifications[0].value.email.to = sstr("team@example.com");
    proj.notifications[0].value.email.subject = sstr("Build report");

    Notification_init(&proj.notifications[1]);
    proj.notifications[1].tag = Notification_slack;
    SlackNotification_init(&proj.notifications[1].value.slack);
    proj.notifications[1].value.slack.channel = sstr("#builds");
    proj.notifications[1].value.slack.message = sstr("Build completed");

    /* Dashboard with maps */
    proj.dashboard.name = sstr("Sprint 42");
    struct json_map_entry_int tc[2];
    tc[0].key = sstr("active");
    tc[0].value = 1;
    tc[1].key = sstr("pending");
    tc[1].value = 1;
    proj.dashboard.task_counts.entries = tc;
    proj.dashboard.task_counts.len = 2;

    struct json_map_entry_sstr_t st[1];
    st[0].key = sstr("view");
    st[0].value = sstr("kanban");
    proj.dashboard.settings.entries = st;
    proj.dashboard.settings.len = 1;

    /* Marshal to pretty JSON */
    sstr_t json = sstr_new();
    check(json_marshal_indent_Project(&proj, 2, 0, json), "marshal project");
    printf("Full project JSON (%zu bytes):\n%s\n",
           sstr_length(json), sstr_cstr(json));

    /* Round-trip: unmarshal → re-marshal */
    struct Project proj2;
    Project_init(&proj2);
    check(json_unmarshal_Project(json, &proj2), "unmarshal project");

    sstr_t json2 = sstr_new();
    check(json_marshal_indent_Project(&proj2, 2, 0, json2),
          "re-marshal project");

    printf("\nRound-trip check: %s\n",
           sstr_compare(json, json2) == 0 ? "IDENTICAL ✓" : "DIFFERENT ✗");

    sstr_free(json);
    sstr_free(json2);

    /* Clean up stack-allocated map entries manually */
    sstr_free(tc[0].key);
    sstr_free(tc[1].key);
    sstr_free(st[0].key);
    sstr_free(st[0].value);
    /* Zero out to prevent double-free by Project_clear */
    proj.dashboard.task_counts.entries = NULL;
    proj.dashboard.task_counts.len = 0;
    proj.dashboard.settings.entries = NULL;
    proj.dashboard.settings.len = 0;

    Project_clear(&proj);
    Project_clear(&proj2);
}

/* ---------- 7. Selective parsing ---------- */

static void example_selective_parsing(void) {
    print_separator("Selective Parsing");

    /* Full JSON input */
    const char* full_json =
        "{\"task_id\":42,\"title\":\"Big task\","
        "\"pri\":\"CRITICAL\",\"status\":\"ACTIVE\","
        "\"assignee\":\"bob\","
        "\"labels\":[\"urgent\",\"api\",\"review\"],"
        "\"estimated_hours\":40}";

    printf("Full JSON: %s\n\n", full_json);

    /* Parse only the id and title fields */
    struct Task task;
    Task_init(&task);

    uint64_t mask[Task_FIELD_MASK_WORD_COUNT] = {0};
    JSON_GEN_C_FIELD_MASK_SET(mask, Task_FIELD_id);
    JSON_GEN_C_FIELD_MASK_SET(mask, Task_FIELD_title);

    sstr_t input = sstr_of(full_json, strlen(full_json));
    check(json_unmarshal_selected_Task(input, &task, mask,
                                        Task_FIELD_MASK_WORD_COUNT),
          "selective unmarshal");

    printf("Selectively parsed (id + title only):\n");
    printf("  id    = %d\n", task.id);
    printf("  title = %s\n", sstr_cstr(task.title));
    printf("  priority = %d (still default MEDIUM=%d, not parsed)\n",
           task.priority, Priority_MEDIUM);
    printf("  tags_len = %d (not parsed)\n", task.tags_len);

    sstr_free(input);
    Task_clear(&task);
}

/* ---------- 8. Parsing from external JSON ---------- */

static void example_parse_external_json(void) {
    print_separator("Parse External JSON");

    /* Simulate receiving JSON from an API with aliases */
    const char* api_response =
        "{"
        "  \"project_name\": \"My App\","
        "  \"version\": 3,"
        "  \"headquarters\": {"
        "    \"street\": \"1 Infinite Loop\","
        "    \"city\": \"Cupertino\","
        "    \"country\": \"US\""
        "  },"
        "  \"tasks\": ["
        "    {\"task_id\": 100, \"title\": \"Ship v3\", \"pri\": \"HIGH\","
        "     \"status\": \"ACTIVE\", \"labels\": [\"release\"],"
        "     \"estimated_hours\": 20}"
        "  ],"
        "  \"metadata\": ["
        "    {\"key\": \"env\", \"value\": \"production\"},"
        "    {\"key\": \"region\", \"value\": \"us-west-2\"},"
        "    {\"key\": \"tier\", \"value\": \"premium\"}"
        "  ],"
        "  \"notifications\": ["
        "    {\"method\": \"webhook\", \"url\": \"https://hook.example.com\","
        "     \"payload\": \"{\\\"event\\\":\\\"deploy\\\"}\","
        "     \"timeout_ms\": 3000}"
        "  ],"
        "  \"dashboard\": {"
        "    \"name\": \"Prod Dashboard\","
        "    \"task_counts\": {\"active\": 1, \"done\": 47},"
        "    \"settings\": {\"refresh_rate\": \"30s\"}"
        "  }"
        "}";

    sstr_t input = sstr_of(api_response, strlen(api_response));
    struct Project proj;
    Project_init(&proj);
    check(json_unmarshal_Project(input, &proj), "unmarshal API response");

    printf("Parsed project: %s (v%u)\n", sstr_cstr(proj.name), proj.version);
    printf("HQ: %s, %s\n",
           sstr_cstr(proj.headquarters.city),
           sstr_cstr(proj.headquarters.country));
    printf("Tasks: %d\n", proj.tasks_len);
    if (proj.tasks_len > 0) {
        printf("  [0] id=%d title=%s priority=%d\n",
               proj.tasks[0].id,
               sstr_cstr(proj.tasks[0].title),
               proj.tasks[0].priority);
    }
    printf("Notifications: %d\n", proj.notifications_len);
    if (proj.notifications_len > 0 &&
        proj.notifications[0].tag == Notification_webhook) {
        printf("  [0] webhook url=%s timeout=%d\n",
               sstr_cstr(proj.notifications[0].value.webhook.url),
               proj.notifications[0].value.webhook.timeout_ms);
    }

    /* Re-marshal as compact JSON */
    sstr_t compact = sstr_new();
    check(json_marshal_Project(&proj, compact), "marshal compact");
    printf("\nCompact output (%zu bytes):\n%s\n",
           sstr_length(compact), sstr_cstr(compact));

    sstr_free(input);
    sstr_free(compact);
    Project_clear(&proj);
}

/* ---------- main ---------- */

int main(void) {
    printf("json-gen-c Advanced Example\n");
    printf("===========================\n");

    example_enums_and_defaults();
    example_optional_nullable();
    example_aliases();
    example_maps();
    example_oneof();
    example_full_project();
    example_selective_parsing();
    example_parse_external_json();

    printf("\nAll examples completed successfully.\n");
    return 0;
}

