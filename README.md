# Yasiv

(Acronym for Yet Another Simple Image Viewer.)  
The goal is to show images on the desktop using windows with no decorations nor borders.

## Features ##

Can open all major image types.  
Very simple manipulation with the mouse (resize & move).  
Singleton application with multiple windows.  
Can show multiple images side by side.  
Snap to borders (screen or other images).  
Save the current layout to a file.  
No pixels used for the interface, as there is none!

## Usage ##

The application is a singleton: invoking it will pass the arguments to the application already opened, if there is one. This is necessary to be able to snap windows to one another.

* From the command line: `Yasiv.exe file1 file2...`
  * If one or multiple images: open all images, each in its own window.
  * If one file is a layout file (.yasiv extension), import it.  
This will close all previously opened windows.
* From inside the application:
  * Press 'N' to open a new, empty window. A dialog box will open to ask for the image to be displayed.
  * Press 'O' to open another image in the existing window.

### Mouse manipulation ###

When interacting with the mouse, the action depends on where the mouse pointer was when the drag started. Basically, move the image by dragging the center of the window, and resize by dragging on corners or the sides. Refer to the following image for the complete list of actions.

![Interaction zones](http://s27.postimg.org/yf59psgfn/Yasiv_mouse.png)

When resizing an image, its original ratio is conserved.

If the **Control key is pressed**, these actions will instead resize the window without modifying the image shown in it. Dragging in the center will move the image inside the window without moving the latter. It is then possible to show only a portion of an image.

### Key shortcuts ###

* 1: Reset to the original size
* F: Resize the image to use the maximum desktop area possible
* H: Resize the image to use the full width of the desktop
* V: Resize the image to use the full height of the desktop
* L: Rotate the image 90° to the left
* R: Rotate the image 90° to the right
* N: Open a new window, a dialog box will ask for the image to be shown
* O: Change the image shown in this window
* E: Export the whole layout to a file
* I: Import a layout from a file, replacing all opened windows
* S: Put all windows of Yasiv on top. Useful when other applications occult some images.
* T: Toggle the use of the alpha channel for this window
* Shift + T: Toggle the use of the alpha channel for all windows
* Escape: close the current window. If it is the last one, the application will also close.
* Q: Close the application and all windows
