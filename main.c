#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define MAX_ITEMS 200

typedef struct {
    char *urls[MAX_ITEMS];
    int count;
} Tracker;

int is_duplicate(Tracker *t, const char *url) {
    for (int i = 0; i < t->count; i++) {
        if (strcmp(t->urls[i], url) == 0) return 1;
    }
    return 0;
}

void add_url(Tracker *t, const char *url) {
    if (t->count < MAX_ITEMS) {
        t->urls[t->count++] = strdup(url);
    }
}

int starts_with(const char *pre, const char *str) {
    return strncmp(pre, str, strlen(pre)) == 0;
}

void strip_tags(char *s) {
    int in_tag = 0;
    char *dest = s;
    while (*s) {
        if (*s == '<') in_tag = 1;
        else if (*s == '>') {
            in_tag = 0;
            s++;
            continue;
        }
        if (!in_tag) *dest++ = *s;
        s++;
    }
    *dest = '\0';
}

// Pro-level cleanup: stops at first newline and squashes internal spaces
void sanitize_title(char *s) {
    char *src = s, *dest = s;
    int was_space = 0;

    // Skip leading whitespace
    while (*src && isspace(*src)) src++;

    while (*src) {
        if (*src == '\n' || *src == '\r') break; // Stop at first newline
        
        if (isspace(*src)) {
            if (!was_space) {
                *dest++ = ' ';
                was_space = 1;
            }
        } else {
            *dest++ = *src;
            was_space = 0;
        }
        src++;
    }
    *dest = '\0';
    
    // Trim trailing space if exists
    if (dest > s && isspace(*(dest - 1))) *(dest - 1) = '\0';
}

void print_xml_safe(const char *s) {
    if (!s) return;
    for (; *s; s++) {
        switch (*s) {
            case '<':  printf("&lt;"); break;
            case '>':  printf("&gt;"); break;
            case '&':  printf("&amp;"); break;
            case '"':  printf("&quot;"); break;
            case '\'': printf("&apos;"); break;
            default:   printf("%c", *s);
        }
    }
}

void parse_html_simple(char *html, const char *base_url) {
    char *p = html;
    Tracker tracker = {0};
    
    while ((p = strstr(p, "<a ")) != NULL) {
        char *href_attr = strstr(p, "href=\"");
        char *tag_end = strchr(p, '>');
        
        if (href_attr && tag_end && href_attr < tag_end) {
            char *href = href_attr + 6;
            char *href_end = strchr(href, '\"');
            
            if (href_end) {
                size_t href_len = href_end - href;
                char *link_url = malloc(href_len + 1);
                strncpy(link_url, href, href_len);
                link_url[href_len] = '\0';
                
                char *text_start = tag_end + 1;
                char *text_end = strstr(text_start, "</a>");
                
                if (text_end) {
                    size_t text_len = text_end - text_start;
                    char *title = malloc(text_len + 1);
                    strncpy(title, text_start, text_len);
                    title[text_len] = '\0';
                    
                    strip_tags(title);
                    sanitize_title(title);

                    // Normalize URL for deduplication
                    char full_url[2048];
                    if (starts_with("http", link_url)) {
                        snprintf(full_url, sizeof(full_url), "%s", link_url);
                    } else {
                        const char *sep = (link_url[0] == '/') ? "" : "/";
                        snprintf(full_url, sizeof(full_url), "%s%s%s", base_url, sep, link_url);
                    }

                    // Heuristics to skip useless/duplicate links
                    if (strlen(title) > 15 && 
                        !starts_with("#", link_url) && 
                        !strstr(title, "Skip to") &&
                        !is_duplicate(&tracker, full_url)) {
                        
                        printf("  <item>\n");
                        printf("    <title><![CDATA[%s]]></title>\n", title);
                        printf("    <link>%s</link>\n", full_url);
                        printf("    <guid isPermaLink=\"false\">%s</guid>\n", full_url);
                        printf("  </item>\n");
                        
                        add_url(&tracker, full_url);
                    }
                    free(title);
                }
                free(link_url);
            }
        }
        p++;
    }

    // Cleanup tracker memory
    for (int i = 0; i < tracker.count; i++) free(tracker.urls[i]);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <url>\n", argv[0]);
        return 1;
    }

    char *url = argv[1];
    size_t url_len = strlen(url);
    if (url_len > 0 && url[url_len-1] == '/') url[url_len-1] = '\0';

    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "curl -sL --user-agent \"feedgen/1.0\" \"%s\"", url);
    
    FILE *fp = popen(cmd, "r");
    if (!fp) return 1;

    size_t capacity = 1024 * 256;
    char *buffer = malloc(capacity);
    size_t total_read = 0, n;

    while ((n = fread(buffer + total_read, 1, capacity - total_read - 1, fp)) > 0) {
        total_read += n;
        if (total_read >= capacity - 1) {
            capacity *= 2;
            buffer = realloc(buffer, capacity);
        }
    }
    buffer[total_read] = '\0';
    pclose(fp);

    time_t now = time(NULL);
    char buf[128];
    strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));

    printf("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
    printf("<rss version=\"2.0\">\n");
    printf("<channel>\n");
    printf("  <title>"); print_xml_safe(url); printf(" Feed</title>\n");
    printf("  <link>%s</link>\n", url);
    printf("  <description>Generated by feedgen</description>\n");
    printf("  <lastBuildDate>%s</lastBuildDate>\n", buf);

    parse_html_simple(buffer, url);

    printf("</channel>\n");
    printf("</rss>\n");

    free(buffer);
    return 0;
}
