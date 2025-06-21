#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <gtk/gtk.h>

#define SERVER_NAME "s1"      // 服务器主机名（非IP地址）
#define SERVER_PORT 8080      // 服务器端口号
#define BUFFER_SIZE 1024      // 缓冲区大小，用于发送和接收数据

// 全局变量
typedef struct {
    GtkWidget *online_button;
    GtkWidget *message_textview;
    GtkWidget *input_entry;
    GtkWidget *send_button;
    GtkTextBuffer *text_buffer;
    int sock;
    pthread_t recv_thread;
    gboolean is_connected;
} ChatData;

ChatData chat_data = {0};

// 用于更新UI的辅助函数
static gboolean update_message_display(gpointer data) {
    char *message = (char *)data;
    gtk_text_buffer_insert_at_cursor(chat_data.text_buffer, message, -1);
    gtk_text_buffer_insert_at_cursor(chat_data.text_buffer, "\n", -1);
    g_free(message);
    return G_SOURCE_REMOVE;
}

// 接收消息的线程函数
void *receive_messages(void *arg) {
    ChatData *data = (ChatData *)arg;
    char buffer[BUFFER_SIZE];

    while (data->is_connected) {
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t bytes_received = recv(data->sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            g_print("与服务器断开连接。\n");
            break;
        }
        
        // 在GTK主线程中更新UI
        char *message = g_strdup(buffer);
        g_idle_add(update_message_display, message);
    }

    return NULL;
}

// 发送消息的回调函数
void send_message_callback(GtkWidget *widget, gpointer data) {
    ChatData *chat = (ChatData *)data;
    
    if (!chat->is_connected) {
        g_print("请先点击上线按钮连接服务器\n");
        return;
    }
    
    const char *message = gtk_entry_get_text(GTK_ENTRY(chat->input_entry));
    if (strlen(message) == 0) {
        return;
    }
    
    // 发送消息到服务器
    ssize_t sent = send(chat->sock, message, strlen(message), 0);
    if (sent < 0) {
        g_print("发送失败\n");
        return;
    }
    
    // 清空输入框
    gtk_entry_set_text(GTK_ENTRY(chat->input_entry), "");
}

// 上线按钮的回调函数
void online_button_callback(GtkWidget *widget, gpointer data) {
    ChatData *chat = (ChatData *)data;
    
    if (chat->is_connected) {
        // 如果已连接，则断开连接
        chat->is_connected = FALSE;
        if (chat->sock != -1) {
            close(chat->sock);
            chat->sock = -1;
        }
        gtk_button_set_label(GTK_BUTTON(chat->online_button), "上线");
        gtk_widget_set_sensitive(chat->input_entry, FALSE);
        gtk_widget_set_sensitive(chat->send_button, FALSE);
        g_print("已断开连接\n");
        return;
    }
    
    // 连接服务器
    struct addrinfo hints, *res, *p;
    char buffer[BUFFER_SIZE];
    
    // 初始化 hints 结构体
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    // 将端口号转换成字符串
    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%d", SERVER_PORT);
    
    // 解析主机名
    int status = getaddrinfo(SERVER_NAME, port_str, &hints, &res);
    if (status != 0) {
        g_print("getaddrinfo 错误: %s\n", gai_strerror(status));
        return;
    }
    
    // 尝试连接
    for (p = res; p != NULL; p = p->ai_next) {
        chat->sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (chat->sock == -1) {
            g_print("socket 创建失败\n");
            continue;
        }
        
        if (connect(chat->sock, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }
        
        close(chat->sock);
        chat->sock = -1;
    }
    
    freeaddrinfo(res);
    
    if (chat->sock == -1) {
        g_print("无法连接到服务器 %s:%d\n", SERVER_NAME, SERVER_PORT);
        return;
    }
    
    // 连接成功
    chat->is_connected = TRUE;
    gtk_button_set_label(GTK_BUTTON(chat->online_button), "下线");
    gtk_widget_set_sensitive(chat->input_entry, TRUE);
    gtk_widget_set_sensitive(chat->send_button, TRUE);
    g_print("已连接到服务器 %s:%d\n", SERVER_NAME, SERVER_PORT);
    
    // 创建接收消息的线程
    if (pthread_create(&chat->recv_thread, NULL, receive_messages, chat) != 0) {
        g_print("接收线程创建失败\n");
        close(chat->sock);
        chat->sock = -1;
        chat->is_connected = FALSE;
        gtk_button_set_label(GTK_BUTTON(chat->online_button), "上线");
        return;
    }
}

// 窗口关闭的回调函数
void window_destroy_callback(GtkWidget *widget, gpointer data) {
    ChatData *chat = (ChatData *)data;
    
    if (chat->is_connected) {
        chat->is_connected = FALSE;
        if (chat->sock != -1) {
            close(chat->sock);
        }
    }
    
    gtk_main_quit();
}

int main(int argc, char **argv) {
    // 初始化GTK
    gtk_init(&argc, &argv);
    
    // 初始化聊天数据结构
    chat_data.sock = -1;
    chat_data.is_connected = FALSE;
    
    // 创建主窗口
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "聊天客户端");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 500);
    g_signal_connect(window, "destroy", G_CALLBACK(window_destroy_callback), &chat_data);
    
    // 创建垂直布局容器
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    
    // 创建上线按钮
    chat_data.online_button = gtk_button_new_with_label("上线");
    g_signal_connect(chat_data.online_button, "clicked", G_CALLBACK(online_button_callback), &chat_data);
    gtk_box_pack_start(GTK_BOX(vbox), chat_data.online_button, FALSE, FALSE, 5);
    
    // 创建消息显示区域
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request(scrolled_window, -1, 300);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 5);
    
    chat_data.text_buffer = gtk_text_buffer_new(NULL);
    chat_data.message_textview = gtk_text_view_new_with_buffer(chat_data.text_buffer);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(chat_data.message_textview), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(chat_data.message_textview), GTK_WRAP_WORD_CHAR);
    gtk_container_add(GTK_CONTAINER(scrolled_window), chat_data.message_textview);
    
    // 创建水平布局容器用于输入框和发送按钮
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
    
    // 创建输入框
    chat_data.input_entry = gtk_entry_new();
    gtk_widget_set_sensitive(chat_data.input_entry, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), chat_data.input_entry, TRUE, TRUE, 0);
    
    // 创建发送按钮
    chat_data.send_button = gtk_button_new_with_label("发送");
    gtk_widget_set_sensitive(chat_data.send_button, FALSE);
    g_signal_connect(chat_data.send_button, "clicked", G_CALLBACK(send_message_callback), &chat_data);
    gtk_box_pack_start(GTK_BOX(hbox), chat_data.send_button, FALSE, FALSE, 0);
    
    // 为输入框添加回车键事件
    g_signal_connect(chat_data.input_entry, "activate", G_CALLBACK(send_message_callback), &chat_data);
    
    // 显示窗口
    gtk_widget_show_all(window);
    
    // 启动GTK主循环
    gtk_main();
    
    return 0;
}
