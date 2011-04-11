///////////////////////////////////////////////////////////////////////////////
// FILE:          MMKeyDispatcher.java
// PROJECT:       Micro-Manager
// SUBSYSTEM:     mmstudio
//-----------------------------------------------------------------------------

// COPYRIGHT:    University of California, San Francisco, 2008

// LICENSE:      This file is distributed under the BSD license.
// License text is included with the source distribution.

// This file is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

// IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES.

package org.micromanager.utils;

import java.awt.KeyEventDispatcher;
import java.awt.event.KeyEvent;
import org.micromanager.MMStudioMainFrame;

/**
 *
 * @author nico
 */
public class MMKeyDispatcher implements KeyEventDispatcher{
   private MMStudioMainFrame gui_;

   public MMKeyDispatcher(MMStudioMainFrame gui) {
      gui_ = gui;
	}

   /*
    * Exclude key events coming from specific sources (like test editors)
    * Only way I could come up with was introspection
    * If there are other areas in the application in which keyevents should not
    * be processed, add those here
    */
   private boolean checkSource(KeyEvent ke) {
      String t = ke.getSource().getClass().toString();
      if (t.equals("class bsh.util.JConsole$1"))
         return false;
      if (t.equals("class org.jeditsyntax.JEditTextArea"))
         return false;
      return true;
   }

   /*
    * 
    */
   public boolean dispatchKeyEvent(KeyEvent ke) {
      if (ke.getID() != KeyEvent.KEY_PRESSED)
         return false;

      // Since all key events in the application go through here
      // we need to efficiently determinne whether or not to deal with this
      // key event will be dealt with.  CheckSource seems relatively expensive
      // so only call this whne the key matches
      char key = ke.getKeyChar();
      // snap
      if (key == 's' && ke.getModifiers() == 0) {
         if (checkSource(ke)) {
            gui_.snapSingleImage();
            return true;
         }
      }
      // toggle live
      if (key == 'l' && ke.getModifiers() == 0) {
         if (checkSource(ke)) {
            if (gui_.getLiveMode())
               gui_.enableLiveMode(false);
            else
               gui_.enableLiveMode(true);
            return true;
         }
      }
      
      // acquire
      if (key == 'a' && ke.getModifiers() == 0) {
         if (checkSource(ke)) {
            gui_.snapAndAddToImage5D(null);
            return true;
         }
      }

      // toggle shutter
      if (key == ' ' && ke.getModifiers() == 0) {
         if (checkSource(ke)) {
            gui_.toggleShutter();
            return true;
         }
      }
      
      return false;
   }
}