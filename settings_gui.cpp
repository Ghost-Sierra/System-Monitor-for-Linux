#include <gtkmm.h>
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>

// Required for fork() and execvp()
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib> // For setenv

// Helper to read the config file into a map
std::map<std::string, std::string> read_config() {
    std::map<std::string, std::string> config;
    std::ifstream config_file("config.ini");
    std::string line;
    while (std::getline(config_file, line)) {
        std::istringstream iss(line);
        std::string key, value;
        if (std::getline(iss, key, '=') && std::getline(iss, value)) {
            key.erase(0, key.find_first_not_of(" \t\n\r"));
            key.erase(key.find_last_not_of(" \t\n\r") + 1);
            value.erase(0, value.find_first_not_of(" \t\n\r"));
            value.erase(value.find_last_not_of(" \t\n\r") + 1);
            config[key] = value;
        }
    }
    return config;
}

// Helper to write the config file from a map
void write_config(const std::map<std::string, std::string>& config) {
    std::ofstream config_file("config.ini");
    config_file << "[Overlay]\n\n";
    for (const auto& pair : config) {
        config_file << pair.first << " = " << pair.second << "\n";
    }
}

class SettingsWindow : public Gtk::Window {
public:
    SettingsWindow();

protected:
    // Helper function to save current settings
    void save_current_settings();

    // Signal handlers:
    void on_save_button_clicked();
    void on_launch_button_clicked();
    void on_close_button_clicked();

    // Member widgets:
    Gtk::Box m_VBox;
    Gtk::Box m_ButtonBox; // NEW: Box for buttons
    Gtk::Label m_PosLabel, m_ColorLabel;
    Gtk::ComboBoxText m_PosComboBox;
    Gtk::ColorButton m_ColorButton;
    
    // NEW Buttons
    Gtk::Button m_SaveButton;
    Gtk::Button m_LaunchButton;
    Gtk::Button m_CloseButton;
    
    std::map<std::string, std::string> m_config;
};

SettingsWindow::SettingsWindow() :
    m_VBox(Gtk::ORIENTATION_VERTICAL, 10),
    m_ButtonBox(Gtk::ORIENTATION_HORIZONTAL, 10),
    m_PosLabel("Overlay Position:"),
    m_ColorLabel("Overlay Color:"),
    m_SaveButton("Save"),
    m_LaunchButton("Save and Launch"),
    m_CloseButton("Close") {
    
    set_title("Overlay Settings");
    set_border_width(12);
    
    // Load existing config
    m_config = read_config();

    // Position ComboBox
    m_PosComboBox.append("top_left", "Top Left");
    m_PosComboBox.append("top_right", "Top Right");
    if (m_config["position"] == "top_right") {
        m_PosComboBox.set_active_id("top_right");
    } else {
        m_PosComboBox.set_active_id("top_left");
    }

    // Color Button
    Gdk::RGBA color;
    color.set_red(std::stof(m_config.count("color_r") ? m_config["color_r"] : "1.0"));
    color.set_green(std::stof(m_config.count("color_g") ? m_config["color_g"] : "1.0"));
    color.set_blue(std::stof(m_config.count("color_b") ? m_config["color_b"] : "0.0"));
    m_ColorButton.set_rgba(color);

    // Pack main widgets
    add(m_VBox);
    m_VBox.pack_start(m_PosLabel, Gtk::PACK_SHRINK);
    m_VBox.pack_start(m_PosComboBox);
    m_VBox.pack_start(m_ColorLabel, Gtk::PACK_SHRINK);
    m_VBox.pack_start(m_ColorButton);
    m_VBox.pack_start(m_ButtonBox, Gtk::PACK_SHRINK); // Pack the button box

    // Pack buttons into their own box
    m_ButtonBox.pack_start(m_SaveButton);
    m_ButtonBox.pack_start(m_LaunchButton);
    m_ButtonBox.pack_start(m_CloseButton);

    // Connect signals
    m_SaveButton.signal_clicked().connect(sigc::mem_fun(*this, &SettingsWindow::on_save_button_clicked));
    m_LaunchButton.signal_clicked().connect(sigc::mem_fun(*this, &SettingsWindow::on_launch_button_clicked));
    m_CloseButton.signal_clicked().connect(sigc::mem_fun(*this, &SettingsWindow::on_close_button_clicked));

    show_all_children();
}

void SettingsWindow::save_current_settings() {
    m_config["position"] = m_PosComboBox.get_active_id();
    
    Gdk::RGBA rgba = m_ColorButton.get_rgba();
    m_config["color_r"] = std::to_string(rgba.get_red());
    m_config["color_g"] = std::to_string(rgba.get_green());
    m_config["color_b"] = std::to_string(rgba.get_blue());

    write_config(m_config);
    std::cout << "Settings saved to config.ini" << std::endl;
}

void SettingsWindow::on_save_button_clicked() {
    save_current_settings();
}

void SettingsWindow::on_launch_button_clicked() {
    // First, save the settings
    save_current_settings();

    // Now, launch the game using fork and exec
    pid_t pid = fork();

    if (pid == -1) {
        // Fork failed
        std::cerr << "Error: Failed to fork process." << std::endl;
    } else if (pid == 0) {
        // --- This is the child process ---
        
        // Set the LD_PRELOAD environment variable for this process only
        setenv("LD_PRELOAD", "./liboverlay.so", 1);
        
        // Prepare arguments for execvp. The list must be null-terminated.
        const char* args[] = {"glxgears", NULL};
        
        // Replace this child process with glxgears
        execvp(args[0], (char* const*)args);
        
        // If execvp returns, it means an error occurred
        perror("execvp failed");
        exit(1);
    }
    // --- This is the parent process (the GUI) ---
    // It simply continues running without waiting for the child.
}

void SettingsWindow::on_close_button_clicked() {
    hide(); // Closes the window
}

int main(int argc, char* argv[]) {
    auto app = Gtk::Application::create(argc, argv, "org.overlay.settings");
    SettingsWindow window;
    return app->run(window);
}