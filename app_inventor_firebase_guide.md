# MIT App Inventor Firebase Guide

This guide will walk you through creating a simple MIT App Inventor application to send data to your Firebase Realtime Database, which can then be read by your ESP32 device.

**Assumptions:**

*   You have a Firebase Realtime Database set up (as per the initial Firebase setup guide).
*   Your Firebase Realtime Database URL is known (e.g., `https://your-project-name-default-rtdb.firebaseio.com/`).
*   Your Firebase database rules are currently in **test mode** (e.g., `".read": true, ".write": true`), allowing public read and write access.
*   The ESP32 is programmed to listen for changes at the Firebase path `/deviceA/buttonState`.
*   The app will write a boolean (`true`/`false`) or a string (`"ON"`/`"OFF"`) to this path.

## 1. Creating a New Project & Basic UI

1.  **Go to MIT App Inventor:** Open your web browser and navigate to [MIT App Inventor](http://ai2.appinventor.mit.edu/). Log in with your Google account.
2.  **Start a New Project:**
    *   Click on **"Start new project"** (or **Projects > Start new project**).
    *   Enter a project name, for example, `ESP32_Control_App`.
    *   Click **"OK"**.
3.  **Design the User Interface (Designer View):**
    *   You are now in the **Designer** view.
    *   **Add a Button:**
        *   From the **"User Interface"** palette on the left, drag a **`Button`** component and drop it onto the Screen1 viewer in the center.
        *   In the **"Properties"** panel on the right, you can change its `Text` property (e.g., to "Send ON") and other properties like `Width` (e.g., "Fill Parent").
    *   **Add a Label (Optional for Feedback):**
        *   Drag a **`Label`** component from the "User Interface" palette and place it below the button.
        *   You can clear its default `Text` property or set it to something like "Status: -"
    *   **Add another Button (for OFF or Toggle):**
        *   Drag another **`Button`** component and place it on the screen.
        *   Change its `Text` property (e.g., "Send OFF" or "Toggle State").

    Your basic UI might look like this:

    ```
    Screen1
      Button1 (Text: "Send ON")
      Button2 (Text: "Send OFF")
      Label1  (Text: "Status: -")
    ```

## 2. Adding and Configuring the FirebaseDB Component

1.  **Add FirebaseDB Component:**
    *   In the **Designer** view, look at the **"Palette"** on the left.
    *   Scroll down to the **"Storage"** section (or it might be under "Experimental" in older versions).
    *   Drag the **`FirebaseDB`** component and drop it anywhere onto the Screen1 viewer.
    *   The `FirebaseDB` component is a **non-visible component**. It will appear below the screen in the "Non-visible components" area.
2.  **Configure FirebaseDB Properties:**
    *   Select the `FirebaseDB1` component (either by clicking it in the "Non-visible components" area or in the "Components" panel).
    *   The **"Properties"** panel on the right will now show the properties for `FirebaseDB1`.
    *   **`FirebaseURL`**:
        *   **Crucial Step:** Set this to your **full Firebase Realtime Database URL**.
        *   Example: `https://esp32-app-control-default-rtdb.firebaseio.com/`
        *   Make sure it includes `https://` at the beginning and a trailing `/` at the end.
    *   **`FirebaseToken`**:
        *   Since your database is in **test mode** (publicly readable and writable), you can **leave this field blank**.
        *   *Note:* If you later secure your database (e.g., with rules like `".read": "auth != null", ".write": "auth != null"`), you would need to implement user authentication and use an ID token here. For simple API Key access or legacy database secrets, the handling in App Inventor can be more complex and is outside the scope of this basic guide. For now, blank is fine for test mode.
    *   **`ProjectBucket`**:
        *   This property acts as a **base path** for all data operations performed by this `FirebaseDB` component.
        *   Your ESP32 is listening to `/deviceA/buttonState`.
        *   Set the `ProjectBucket` to `deviceA`.
        *   **Explanation:** When you later use the `StoreValue` block with a `tag` of `buttonState`, the data will automatically be written to `deviceA/buttonState` in your Firebase database. If you left `ProjectBucket` blank, you would need to use `deviceA/buttonState` as the `tag` in your `StoreValue` block. Using `ProjectBucket` simplifies the tags in your blocks.

## 3. Block Logic

Now, let's define how the app behaves when the buttons are pressed.

1.  **Switch to Blocks View:**
    *   In the top-right corner of the App Inventor interface, click the **"Blocks"** button.

2.  **Simple ON/OFF Button Logic:**

    *   **Button1 (Send ON) Click Event:**
        *   In the "Blocks" panel on the left, find `Button1` and click on it.
        *   Drag the `when Button1.Click do` block onto the workspace.
        *   From the `FirebaseDB1` drawer, drag the `call FirebaseDB1.StoreValue` block and snap it inside the `Button1.Click` block.
        *   **`tag`**:
            *   From the "Text" blocks drawer, drag a `""` (empty text string) block.
            *   Type `buttonState` into the text block.
            *   Connect this to the `tag` socket of the `StoreValue` block.
        *   **`valueToStore`**:
            *   **Option 1 (Sending a boolean `true`):**
                *   From the "Logic" blocks drawer, drag a `true` block and connect it to the `valueToStore` socket.
            *   **Option 2 (Sending a string `"ON"`):**
                *   From the "Text" blocks drawer, drag a `""` block, type `ON` into it, and connect it.
                *   **Choose one option and be consistent with what your ESP32 code expects.**
        *   **(Optional) Update Label1:**
            *   From the `Label1` drawer, drag the `set Label1.Text to` block.
            *   From the "Text" drawer, drag a `""` block, type `Sent: ON` (or `true`), and connect it.

    *   **Button2 (Send OFF) Click Event:**
        *   Repeat the process for `Button2`.
        *   Drag out a `when Button2.Click do` block.
        *   Add a `call FirebaseDB1.StoreValue` block.
        *   `tag`: Use a text block with `buttonState`.
        *   `valueToStore`:
            *   **Option 1 (Sending a boolean `false`):** Use a `false` block from "Logic".
            *   **Option 2 (Sending a string `"OFF"`):** Use a text block with `OFF`.
        *   **(Optional) Update Label1:** Set `Label1.Text` to `Sent: OFF` (or `false`).

    **Text Representation of Simple ON/OFF Blocks:**

    ```
    when Button1.Click do
      call FirebaseDB1.StoreValue
        tag = "buttonState"
        valueToStore = true  // or "ON"
      set Label1.Text to "Status: Sent ON"

    when Button2.Click do
      call FirebaseDB1.StoreValue
        tag = "buttonState"
        valueToStore = false // or "OFF"
      set Label1.Text to "Status: Sent OFF"
    ```
    *(You can find images of these blocks by searching "App Inventor Firebase StoreValue blocks" online if needed.)*

3.  **Alternative: Toggle Button Logic (More Advanced)**

    If you prefer a single button to toggle the state:

    *   **Initialize a Global Variable:**
        *   From the "Variables" drawer, drag an `initialize global name to` block.
        *   Change `name` to `isButtonOn`.
        *   From the "Logic" drawer, drag a `false` block and attach it.
            ```
            initialize global isButtonOn to false
            ```
    *   **Button Click Event (e.g., using Button1, rename its text to "Toggle State"):**
        *   Drag out a `when Button1.Click do` block.
        *   **Invert the global variable:**
            *   From the "Variables" drawer, drag a `set global isButtonOn to` block.
            *   From the "Logic" drawer, drag a `not` block.
            *   From the "Variables" drawer, drag a `get global isButtonOn` block and attach it to the `not` block.
                ```
                set global isButtonOn to (not (get global isButtonOn))
                ```
        *   **Store the new state in Firebase:**
            *   Drag a `call FirebaseDB1.StoreValue` block.
            *   `tag`: Text block with `buttonState`.
            *   `valueToStore`: Drag a `get global isButtonOn` block.
        *   **Update Button Text and/or Label (Conditional Logic):**
            *   From the "Control" drawer, drag an `if then else` block.
            *   **Condition:** Drag a `get global isButtonOn` block.
            *   **`then` part (if `isButtonOn` is true):**
                *   Set `Button1.Text` to "Turn OFF".
                *   Set `Label1.Text` to "Status: ON Sent".
            *   **`else` part (if `isButtonOn` is false):**
                *   Set `Button1.Text` to "Turn ON".
                *   Set `Label1.Text` to "Status: OFF Sent".

    **Text Representation of Toggle Blocks:**

    ```
    initialize global isButtonOn to false

    when Button1.Click do  // (Assuming Button1 text is "Toggle State")
      set global isButtonOn to (not (get global isButtonOn))
      call FirebaseDB1.StoreValue
        tag = "buttonState"
        valueToStore = (get global isButtonOn)
      if (get global isButtonOn) then
        set Button1.Text to "Turn OFF"
        set Label1.Text to "Status: ON Sent"
      else
        set Button1.Text to "Turn ON"
        set Label1.Text to "Status: OFF Sent"
    ```

## 4. Testing the Application

1.  **Ensure ESP32 is Ready:**
    *   Your ESP32 (from the previous ESP32 Firebase guide) should be programmed, powered on, connected to Wi-Fi, and successfully connected to Firebase.
    *   Have its Serial Monitor open in the Arduino IDE (or other terminal program) so you can see the messages it prints when data is received from Firebase.

2.  **Connect with MIT AI Companion (Recommended for Live Testing):**
    *   **On your Android device:** Install the "MIT AI2 Companion" app from the Google Play Store.
    *   **In MIT App Inventor (on your computer):**
        *   Click on **"Connect"** in the top menu.
        *   Select **"AI Companion"**.
        *   A dialog box will appear with a QR code and a 6-character code.
    *   **On your Android device:**
        *   Open the MIT AI2 Companion app.
        *   Either scan the QR code using the app's scanner or type in the 6-character code and click "Connect with code".
    *   Your app should now load and appear on your Android device.

3.  **Test the Interaction:**
    *   **Press the button(s) in your app** (e.g., "Send ON", "Send OFF", or the "Toggle State" button).
    *   **Check Firebase Console:**
        *   Open your Firebase project in a web browser.
        *   Navigate to **Build > Realtime Database**.
        *   You should see the data at the path `deviceA/buttonState` change according to the button you pressed in the app (e.g., `true`, `false`, `"ON"`, or `"OFF"`).
    *   **Check ESP32 Serial Monitor:**
        *   The Serial Monitor for your ESP32 should print messages indicating that it has received a data update from Firebase. The `streamCallback` function in the ESP32 code will show the new value for `/deviceA/buttonState`.

If everything is configured correctly, pressing the button in your App Inventor app will change the data in Firebase, which will then be detected by your ESP32 in real-time. This completes the loop of controlling your ESP32 from a custom mobile app via Firebase!
