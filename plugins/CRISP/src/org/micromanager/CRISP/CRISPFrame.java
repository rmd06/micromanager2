/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * CRISPFrame.java
 *
 * Created on Nov 15, 2011, 5:33:49 PM
 */

package org.micromanager.CRISP;

import java.text.NumberFormat;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.prefs.Preferences;
import mmcorej.CMMCore;
import mmcorej.DeviceType;
import org.micromanager.api.DeviceControlGUI;
import org.micromanager.api.ScriptInterface;

/**
 *
 * @author Valelab
 */
public class CRISPFrame extends javax.swing.JFrame {

    private final ScriptInterface gui_;
    private final DeviceControlGUI dGui_;
    private final CMMCore core_;
    private Preferences prefs_;
    private final NumberFormat nf_;
    private String CRISP_;


    /** Creates new form CRISPFrame */
    public CRISPFrame(ScriptInterface gui)  {
       gui_ = gui;
       dGui_ = (DeviceControlGUI) gui;
       core_ = gui.getMMCore();
       nf_ = NumberFormat.getInstance();
       prefs_ = Preferences.userNodeForPackage(this.getClass());
       CRISP_ = "";

       mmcorej.StrVector afs =
               core_.getLoadedDevicesOfType(DeviceType.AutoFocusDevice);
       boolean found = false;
       for (String af : afs) {
         try {
            if (core_.hasProperty(af, "Description")) {
               if (core_.getProperty(af, "Description").equals("ASI CRISP Autofocus adapter")) {
                  found = true;
                  CRISP_ = af;
               }
            }
         } catch (Exception ex) {
            Logger.getLogger(CRISPFrame.class.getName()).log(Level.SEVERE, null, ex);
         }
       }

       if (!found) {
          gui_.showError("This plugin needs the ASI CRISP Autofcous");
          throw new IllegalArgumentException("This plugin needs at least one camera");
       }


       initComponents();
    }

    /** This method is called from within the constructor to
     * initialize the form.
     * WARNING: Do NOT modify this code. The content of this method is
     * always regenerated by the Form Editor.
     */
    @SuppressWarnings("unchecked")
   // <editor-fold defaultstate="collapsed" desc="Generated Code">//GEN-BEGIN:initComponents
   private void initComponents() {

      LockButton = new javax.swing.JToggleButton();
      CalibrateButton = new javax.swing.JButton();
      CurveButton = new javax.swing.JButton();

      setDefaultCloseOperation(javax.swing.WindowConstants.EXIT_ON_CLOSE);

      LockButton.setText("Lock");
      LockButton.addActionListener(new java.awt.event.ActionListener() {
         public void actionPerformed(java.awt.event.ActionEvent evt) {
            LockButtonActionPerformed(evt);
         }
      });

      CalibrateButton.setText("Calibrate");

      CurveButton.setText("Curve");

      javax.swing.GroupLayout layout = new javax.swing.GroupLayout(getContentPane());
      getContentPane().setLayout(layout);
      layout.setHorizontalGroup(
         layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addGroup(javax.swing.GroupLayout.Alignment.TRAILING, layout.createSequentialGroup()
            .addContainerGap()
            .addComponent(LockButton, javax.swing.GroupLayout.PREFERRED_SIZE, 86, javax.swing.GroupLayout.PREFERRED_SIZE)
            .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED, 74, Short.MAX_VALUE)
            .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.TRAILING, false)
               .addComponent(CurveButton, javax.swing.GroupLayout.Alignment.LEADING, javax.swing.GroupLayout.DEFAULT_SIZE, javax.swing.GroupLayout.DEFAULT_SIZE, Short.MAX_VALUE)
               .addComponent(CalibrateButton, javax.swing.GroupLayout.Alignment.LEADING, javax.swing.GroupLayout.DEFAULT_SIZE, 86, Short.MAX_VALUE))
            .addGap(26, 26, 26))
      );
      layout.setVerticalGroup(
         layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
         .addGroup(layout.createSequentialGroup()
            .addGap(21, 21, 21)
            .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.BASELINE)
               .addComponent(LockButton)
               .addComponent(CalibrateButton))
            .addGap(18, 18, 18)
            .addComponent(CurveButton)
            .addContainerGap(23, Short.MAX_VALUE))
      );

      pack();
   }// </editor-fold>//GEN-END:initComponents

    private void LockButtonActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_LockButtonActionPerformed
       if (LockButton.isSelected()) {
          LockButton.setText("Lock");
          // unlock the device
          LockButton.setSelected(false);
       }
       else {
          LockButton.setText("Unlock");
          // lock the device
          LockButton.setSelected(true);
       }
    }//GEN-LAST:event_LockButtonActionPerformed

  public void safePrefs() {
      // prefs_.putInt(FRAMEXPOS, this.getX());
      // prefs_.putInt(FRAMEYPOS, this.getY());

   }


   // Variables declaration - do not modify//GEN-BEGIN:variables
   private javax.swing.JButton CalibrateButton;
   private javax.swing.JButton CurveButton;
   private javax.swing.JToggleButton LockButton;
   // End of variables declaration//GEN-END:variables

}