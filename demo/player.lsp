(= Player (class
  :jump 0
  :dir 6

  :step (func ()
    (if (key "left")  (do (= :dir 6) (super':move -0.15 0))
        (key "right") (do (= :dir 0) (super':move +0.15 0))
    )
    
    (if (and (key "up") (< 0 :jump)) (do
          (if (is :jump 6) (play jump.wav nil))
          (super':move 0 -1)
          (= :jump (- :jump 1))
        )
        (super':on-ground?) (= :jump 6)
    )

    (super':step)

    (let x (super':get-x)
         y (super':get-y)
    )

    (camera (- 64 (clamp (- x 4) 64 (- (* 128 8) 64)))
            (- 64 (clamp (- y 4) 64 (- (*  24 8) 64)))
    )

    (if (super':on-ground?)
      (do
        (let dx (super':get-dx)
             frame (* 5 (time))
        )
        (if (< (abs dx) 0.3)
          ; idle animation
          (draw x y player.bmp :dir)
          ; walk animation
          (draw x y player.bmp (+ 2 :dir (% frame 4)))
        )
      )
      (draw x y player.bmp (+ 1 :dir))
    )
  )

  :kill (func ()
    (= player nil)
    (play hurt.wav nil)
    (stop music.wav)
    (make-puffts (super':get-x) (super':get-y) 9)
  )
 
  :smashed? (func (e)
    (let x0 (super':get-x) y0 (super':get-y) s0 (super':get-size)
         x1 (e':get-x)     y1 (e':get-y)     s1 (e':get-size)
    )
    (if (collide? x0 y0 s0 x1 y1 s1)
      (if (<= (* s0 0.65) (- y1 y0))
        (do (e':kill) (super':bounce))
        (:kill)
      )
    )
  )
 
  init (func (x y)
    (= super (Actor x y 8))
    (= player self)
  )
)) 
