#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>
#include <signal.h>
#include <string.h>

static volatile sig_atomic_t cancel = 0;

void signal_handler(int signum)
{
	cancel = 1;
}

int main()
{
  struct sigaction action;

  //------------------------------------------------------------------------------
  //Opening the DRI device
  //------------------------------------------------------------------------------

  int dri_fd  = open("/dev/dri/card0",O_RDWR | O_CLOEXEC);

  //------------------------------------------------------------------------------
  //Kernel Mode Setting (KMS)
  //------------------------------------------------------------------------------

  uint32_t res_fb_buf[10]={0},
      res_crtc_buf[10]={0},
      res_conn_buf[10]={0},
      res_enc_buf[10]={0};
  struct drm_mode_crtc saved_crtc[10]={0};

  struct drm_mode_card_res res={0};

  memset(&action, 0, sizeof(struct sigaction));
  action.sa_handler = signal_handler;
  sigaction(SIGTERM, &action, NULL);
  sigaction(SIGINT, &action, NULL);

  //Become the "master" of the DRI device
  printf ("Master: %d\n", ioctl(dri_fd, DRM_IOCTL_SET_MASTER, 0));

  //Get resource counts
  printf ("Get resources: %d\n", ioctl(dri_fd, DRM_IOCTL_MODE_GETRESOURCES, &res));

  res.fb_id_ptr=(uint64_t)res_fb_buf;
  res.crtc_id_ptr=(uint64_t)res_crtc_buf;
  res.connector_id_ptr=(uint64_t)res_conn_buf;
  res.encoder_id_ptr=(uint64_t)res_enc_buf;
  //Get resource IDs
  printf ("Get resources: %d\n", ioctl(dri_fd, DRM_IOCTL_MODE_GETRESOURCES, &res));

  printf("fb: %d, crtc: %d, conn: %d, enc: %d\n",res.count_fbs,res.count_crtcs,res.count_connectors,res.count_encoders);

  void *fb_base[10];
  long fb_w[10] ={0};
  long fb_h[10] ={0};

  //Loop though all available connectors
  int i;
  for (i=0;i<res.count_connectors;i++)
    {
      struct drm_mode_modeinfo conn_mode_buf[20]={0};
      uint64_t	conn_prop_buf[20]={0},
          conn_propval_buf[20]={0},
          conn_enc_buf[20]={0};

      struct drm_mode_get_connector conn={0};

      printf ("***** Getting connector [%d] = %d\n", i, res_conn_buf[i]);
      conn.connector_id=res_conn_buf[i];

      printf ("Get connector: %d\n", ioctl(dri_fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn));	//get connector resource counts
      printf("enc: %d, modes: %d, props: %d, conn : %X\n",conn.count_encoders,conn.count_modes,conn.count_props, conn.connection);
      printf("conn id: %X, conn type: %X, conn type id: %X, enc id: %X\n",
          conn.connector_id,conn.connector_type,conn.connector_type_id, conn.encoder_id);
      conn.modes_ptr=(uint64_t)conn_mode_buf;
      conn.props_ptr=(uint64_t)conn_prop_buf;
      conn.prop_values_ptr=(uint64_t)conn_propval_buf;
      conn.encoders_ptr=(uint64_t)conn_enc_buf;
      printf ("Get connector: %d\n", ioctl(dri_fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn));	//get connector resources
      printf("enc: %d, modes: %d, props: %d, conn : %X\n",conn.count_encoders,conn.count_modes,conn.count_props, conn.connection);
      printf("conn id: %X, conn type: %X, conn type id: %X, enc id: %X\n",
          conn.connector_id,conn.connector_type,conn.connector_type_id, conn.encoder_id);

      //Check if the connector is OK to use (connected to something)
      if (conn.count_encoders<1 || conn.count_modes<1 || !conn.encoder_id || !conn.connection)
        {
          printf("Not connected\n");
          continue;
        }

      //------------------------------------------------------------------------------
      //Creating a dumb buffer
      //------------------------------------------------------------------------------
      struct drm_mode_create_dumb create_dumb={0};
      struct drm_mode_map_dumb map_dumb={0};
      struct drm_mode_fb_cmd cmd_dumb={0};

      //If we create the buffer later, we can get the size of the screen first.
      //This must be a valid mode, so it's probably best to do this after we find
      //a valid crtc with modes.
      create_dumb.width = conn_mode_buf[0].hdisplay;
      create_dumb.height = conn_mode_buf[0].vdisplay;
      create_dumb.bpp = 32;
      create_dumb.flags = 0;
      create_dumb.pitch = 0;
      create_dumb.size = 0;
      create_dumb.handle = 0;
      printf ("Create Dumb: %d\n", ioctl(dri_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb));

      printf ("Dumb FB created : %dx%d pitch %d\n", create_dumb.width, create_dumb.height, create_dumb.pitch);
      cmd_dumb.width=create_dumb.width;
      cmd_dumb.height=create_dumb.height;
      cmd_dumb.bpp=create_dumb.bpp;
      cmd_dumb.pitch=create_dumb.pitch;
      cmd_dumb.depth=24;
      cmd_dumb.handle=create_dumb.handle;
      printf ("Add FB: %d\n", ioctl(dri_fd,DRM_IOCTL_MODE_ADDFB,&cmd_dumb));

      map_dumb.handle=create_dumb.handle;
      printf ("Map Dumb: %d\n", ioctl(dri_fd,DRM_IOCTL_MODE_MAP_DUMB,&map_dumb));

      fb_base[i] = mmap(0, create_dumb.size, PROT_READ | PROT_WRITE, MAP_SHARED, dri_fd, map_dumb.offset);
      fb_w[i]=create_dumb.width;
      fb_h[i]=create_dumb.height;

      //------------------------------------------------------------------------------
      //Kernel Mode Setting (KMS)
      //------------------------------------------------------------------------------

      printf("%d : mode: %d, prop: %d, enc: %d\n",conn.connection,conn.count_modes,conn.count_props,conn.count_encoders);
      printf("modes: %dx%d FB: %d\n",conn_mode_buf[0].hdisplay,conn_mode_buf[0].vdisplay,fb_base[i]);
      printf("FB id: %d\n", cmd_dumb.fb_id);

      struct drm_mode_get_encoder enc={0};

      enc.encoder_id=conn.encoder_id;
      printf ("Get encoder: %d\n", ioctl(dri_fd, DRM_IOCTL_MODE_GETENCODER, &enc));	//get encoder
      printf ("id: %d - type : %d, crtc_id : %d, possible crtcs : %d - clones : %d\n",
          enc.encoder_id, enc.encoder_type, enc.crtc_id, enc.possible_crtcs, enc.possible_clones);
      struct drm_mode_crtc crtc={0};

      crtc.crtc_id=enc.crtc_id;
      printf ("Get CRTC: %d\n", ioctl(dri_fd, DRM_IOCTL_MODE_GETCRTC, &crtc));

      saved_crtc[i] = crtc;
      printf ("id: %d - fb id : %d, count connectors : %d - %X, x : %d - y : %d - mode_valid : %d  size : %dx%d\n",
          crtc.crtc_id, crtc.fb_id, crtc.count_connectors, crtc.set_connectors_ptr, crtc.x, crtc.y, crtc.mode_valid, crtc.mode.hdisplay, crtc.mode.vdisplay);

      crtc.fb_id=cmd_dumb.fb_id;
      crtc.set_connectors_ptr=(uint64_t)&res_conn_buf[i];
      crtc.count_connectors=1;
      crtc.mode=conn_mode_buf[0];
      crtc.mode_valid=1;
      printf ("Set CRTC: %d\n", ioctl(dri_fd, DRM_IOCTL_MODE_SETCRTC, &crtc));
    }

  //Get resource IDs
  printf ("Get resources: %d\n", ioctl(dri_fd, DRM_IOCTL_MODE_GETRESOURCES, &res));

  printf("fb: %d, crtc: %d, conn: %d, enc: %d\n",res.count_fbs,res.count_crtcs,res.count_connectors,res.count_encoders);

  int x,y;
  while (!cancel)
    {
      int j;
      for (j=0;j<res.count_connectors;j++)
        {
          int col=(rand()%0x00ffffff)&0x00ff00ff;
          for (y=0;y<fb_h[j];y++) {
            for (x=0;x<fb_w[j];x++)
              {
                int location=y*(fb_w[j]) + x;
                *(((uint32_t*)fb_base[j])+location)=col;
              }
          }
        }
      usleep(100000);
    }

  for (i=0;i<res.count_connectors;i++) {
    if (saved_crtc[i].crtc_id == 0)
      continue;
    printf ("Setting CRTC back to : \n");
    printf ("id: %d - fb id : %d, count connectors : %d, x : %d - y : %d - mode_valid : %d  size : %dx%d\n",
        saved_crtc[i].crtc_id, saved_crtc[i].fb_id, saved_crtc[i].count_connectors, saved_crtc[i].x, saved_crtc[i].y, saved_crtc[i].mode_valid, saved_crtc[i].mode.hdisplay, saved_crtc[i].mode.vdisplay);
    saved_crtc[i].set_connectors_ptr=(uint64_t)&res_conn_buf[i];
    saved_crtc[i].count_connectors=1;
    printf ("Set CRTC: %d\n", ioctl(dri_fd, DRM_IOCTL_MODE_SETCRTC, &saved_crtc[i]));
  }
  //Stop being the "master" of the DRI device
  printf ("Drop master : %d\n", ioctl(dri_fd, DRM_IOCTL_DROP_MASTER, 0));

  return 0;
}
